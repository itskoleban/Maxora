#include "pawn_bridge.hpp"

#include <bit>
#include <limits>
#include <stdexcept>

namespace mmdb {
namespace {
PawnBridge* route = nullptr;
static_assert(sizeof(cell) == 4, "The MMDB Pawn ABI requires 32-bit cells");

class Memory {
public:
	explicit Memory(AMX* amx) : amx_(amx) {}
	bool parameters(const cell* pointer, std::size_t count) const noexcept {
		if (!amx_ || !pointer) return false;
		unsigned char* data = amx_->data;
		if (!data && amx_->base) {
			const auto* header = reinterpret_cast<const AMX_HEADER*>(amx_->base);
			if (header->dat < 0) return false;
			data = amx_->base + header->dat;
		}
		if (!data) return false;
		const auto base = reinterpret_cast<std::uintptr_t>(data);
		const auto address = reinterpret_cast<std::uintptr_t>(pointer);
		if (address < base || address - base > static_cast<std::uintptr_t>(std::numeric_limits<cell>::max()))
			return false;
		const auto offset = static_cast<cell>(address - base);
		// The interpreter passes the parameter block at the saved stack pointer.
		return offset == amx_->stk && cells(offset, count) == pointer;
	}
	cell* cells(cell address, std::size_t count = 1) const noexcept {
		if (!amx_ || address < 0 || address % 4 != 0 || count == 0 ||
			count > static_cast<std::size_t>(std::numeric_limits<cell>::max()) / 4)
			return nullptr;
		const auto start = static_cast<std::uint32_t>(address);
		const auto length = static_cast<std::uint32_t>(count * 4);
		const auto end = static_cast<std::uint64_t>(start) + length;
		if (amx_->hea < 0 || amx_->stk < amx_->hea || amx_->stp < amx_->stk ||
			!((end <= static_cast<std::uint32_t>(amx_->hea)) ||
			  (start >= static_cast<std::uint32_t>(amx_->stk) &&
			   end <= static_cast<std::uint32_t>(amx_->stp))))
			return nullptr;
		unsigned char* data = amx_->data;
		if (!data && amx_->base) {
			auto* header = reinterpret_cast<const AMX_HEADER*>(amx_->base);
			if (header->dat < 0) return nullptr;
			data = amx_->base + header->dat;
		}
		return data ? reinterpret_cast<cell*>(data + start) : nullptr;
	}
	Status string(cell address, std::size_t limit, std::string& out) const {
		out.clear();
		for (std::size_t i = 0; i <= limit; ++i) {
			const auto at = static_cast<std::int64_t>(address) + static_cast<std::int64_t>(i * 4);
			if (at > std::numeric_limits<cell>::max()) return Status::InvalidArgument;
			const auto* p = cells(static_cast<cell>(at));
			if (!p || *p < 0 || *p > 255) return Status::InvalidArgument;
			if (*p == 0) return valid_utf8(out) ? Status::Ok : Status::InvalidUtf8;
			if (i == limit) return Status::InvalidArgument;
			out.push_back(static_cast<char>(*p));
		}
		return Status::InvalidArgument;
	}
	Status output(cell address, cell capacity, cell required, std::string_view text) const noexcept {
		auto* needed = cells(required);
		if (!needed || capacity < 1 || capacity > 1048576 ||
			text.size() >= static_cast<std::size_t>(std::numeric_limits<cell>::max()))
			return Status::InvalidArgument;
		auto* destination = cells(address, static_cast<std::size_t>(capacity));
		if (!destination) return Status::InvalidArgument;
		const auto begin = static_cast<std::int64_t>(address);
		const auto end = begin + static_cast<std::int64_t>(capacity) * 4;
		if (required >= begin && required < end) return Status::InvalidArgument;
		*needed = static_cast<cell>(text.size() + 1);
		if (text.size() >= static_cast<std::size_t>(capacity)) return Status::BufferTooSmall;
		for (std::size_t i = 0; i < text.size(); ++i)
			destination[i] = static_cast<unsigned char>(text[i]);
		destination[text.size()] = 0;
		return Status::Ok;
	}
private:
	AMX* amx_;
};
struct Context {
	Service& service;
	AMX* amx;
	Memory memory;
	const cell* p;
};

template <std::size_t Count, typename Function>
cell invoke(AMX* amx, const cell* params, Function function) noexcept {
	if (!route || !amx || !params || !route->service().attached(amx))
		return static_cast<cell>(Status::InvalidArgument);
	const Memory memory(amx);
	// The interpreter passes the stack pointer, but the count cell is script
	// controlled. Validate the entire argument block before reading it.
	if (!memory.parameters(params, Count + 1) || params[0] != static_cast<cell>(Count * sizeof(cell)))
		return static_cast<cell>(Status::InvalidArgument);
	try {
		return static_cast<cell>(function(Context{route->service(), amx, memory, params}));
	} catch (...) {
		return static_cast<cell>(Status::InternalError);
	}
}

Status get_database(const Context& c, std::shared_ptr<const Database>& database) {
	std::string name;
	const auto status = c.memory.string(c.p[1], Service::max_name, name);
	return status == Status::Ok ? c.service.database(c.amx, name, database) : status;
}

Status get_record(const Context& c, Lookup& lookup) {
	std::string name, ip;
	auto status = c.memory.string(c.p[1], Service::max_name, name);
	if (status != Status::Ok) return status;
	status = c.memory.string(c.p[2], 45, ip);
	return status == Status::Ok ? c.service.lookup(c.amx, name, ip, lookup) : status;
}

Status get_value(const Context& c, Lookup& lookup, Value& value) {
	std::string path;
	const auto status = c.memory.string(c.p[3], 1024, path);
	if (status != Status::Ok) return status;
	// Validate the path even when the address is not present.
	Path parsed;
	const auto path_status = parsed.parse(path);
	if (path_status != Status::Ok) return path_status;
	const auto record_status = get_record(c, lookup);
	return record_status == Status::Ok ? lookup.get(parsed, value) : record_status;
}

template <auto Operation>
cell AMX_NATIVE_CALL open_database(AMX* amx, const cell* p) {
	return invoke<2>(amx, p, [](const Context& c) {
		std::string name, path;
		auto status = c.memory.string(c.p[1], Service::max_name, name);
		if (status != Status::Ok) return status;
		status = c.memory.string(c.p[2], 4095, path);
		return status == Status::Ok ? (c.service.*Operation)(c.amx, name, path) : status;
	});
}

cell AMX_NATIVE_CALL unload(AMX* amx, const cell* p) {
	return invoke<1>(amx, p, [](const Context& c) {
		std::string name;
		const auto status = c.memory.string(c.p[1], Service::max_name, name);
		return status == Status::Ok ? c.service.unload(c.amx, name) : status;
	});
}

cell AMX_NATIVE_CALL is_loaded(AMX* amx, const cell* p) {
	return invoke<2>(amx, p, [](const Context& c) {
		auto* out = c.memory.cells(c.p[2]);
		if (!out) return Status::InvalidArgument;
		std::shared_ptr<const Database> database;
		const auto status = get_database(c, database);
		if (status != Status::Ok && status != Status::NotLoaded) return status;
		*out = status == Status::Ok ? 1 : 0;
		return Status::Ok;
	});
}

cell AMX_NATIVE_CALL has_record(AMX* amx, const cell* p) {
	return invoke<3>(amx, p, [](const Context& c) {
		auto* out = c.memory.cells(c.p[3]);
		if (!out) return Status::InvalidArgument;
		Lookup lookup;
		const auto status = get_record(c, lookup);
		if (status != Status::Ok && status != Status::NoRecord) return status;
		*out = status == Status::Ok ? 1 : 0;
		return Status::Ok;
	});
}

template <bool Text>
cell AMX_NATIVE_CALL get_string(AMX* amx, const cell* p) {
	return invoke<6>(amx, p, [](const Context& c) {
		Lookup lookup;
		Value value;
		auto status = get_value(c, lookup, value);
		if (status != Status::Ok) return status;
		if constexpr (Text) {
			std::string text;
			status = value.text(text);
			return status == Status::Ok ? c.memory.output(c.p[4], c.p[5], c.p[6], text) : status;
		} else {
			std::string_view text;
			status = value.string(text);
			return status == Status::Ok ? c.memory.output(c.p[4], c.p[5], c.p[6], text) : status;
		}
	});
}

enum class Scalar { Integer, Floating, Boolean, Type, Size };

template <Scalar Kind>
cell AMX_NATIVE_CALL get_scalar(AMX* amx, const cell* p) {
	return invoke<4>(amx, p, [](const Context& c) {
		auto* out = c.memory.cells(c.p[4]);
		if (!out) return Status::InvalidArgument;
		Lookup lookup;
		Value value;
		auto status = get_value(c, lookup, value);
		if (status != Status::Ok) return status;
		std::int32_t number = 0;
		if constexpr (Kind == Scalar::Integer) {
			status = value.integer(number);
		} else if constexpr (Kind == Scalar::Floating) {
			float converted = 0;
			status = value.floating(converted);
			number = std::bit_cast<std::int32_t>(converted);
		} else if constexpr (Kind == Scalar::Boolean) {
			bool converted = false;
			status = value.boolean(converted);
			number = converted ? 1 : 0;
		} else if constexpr (Kind == Scalar::Type) {
			number = static_cast<std::int32_t>(value.data.type);
		} else {
			const auto type = value.data.type;
			if (type != MMDB_DATA_TYPE_MAP && type != MMDB_DATA_TYPE_ARRAY &&
				type != MMDB_DATA_TYPE_UTF8_STRING && type != MMDB_DATA_TYPE_BYTES)
				return Status::TypeMismatch;
			if (value.data.data_size > static_cast<std::uint32_t>(std::numeric_limits<cell>::max()))
				return Status::OutOfRange;
			number = static_cast<std::int32_t>(value.data.data_size);
		}
		if (status == Status::Ok) *out = number;
		return status;
	});
}

cell AMX_NATIVE_CALL has_field(AMX* amx, const cell* p) {
	return invoke<4>(amx, p, [](const Context& c) {
		auto* out = c.memory.cells(c.p[4]);
		if (!out) return Status::InvalidArgument;
		Lookup lookup;
		Value value;
		const auto status = get_value(c, lookup, value);
		if (status != Status::Ok && status != Status::NoField) return status;
		*out = status == Status::Ok ? 1 : 0;
		return Status::Ok;
	});
}

cell AMX_NATIVE_CALL prefix(AMX* amx, const cell* p) {
	return invoke<3>(amx, p, [](const Context& c) {
		auto* out = c.memory.cells(c.p[3]);
		if (!out) return Status::InvalidArgument;
		Lookup lookup;
		const auto status = get_record(c, lookup);
		if (status == Status::Ok) *out = lookup.prefix;
		return status;
	});
}

template <bool Epoch>
cell AMX_NATIVE_CALL metadata_string(AMX* amx, const cell* p) {
	return invoke<4>(amx, p, [](const Context& c) {
		std::shared_ptr<const Database> database;
		const auto status = get_database(c, database);
		if (status != Status::Ok) return status;
		const auto& metadata = database->raw().metadata;
		if constexpr (Epoch) {
			return c.memory.output(c.p[2], c.p[3], c.p[4], std::to_string(metadata.build_epoch));
		} else {
			return c.memory.output(c.p[2], c.p[3], c.p[4], metadata.database_type);
		}
	});
}

cell AMX_NATIVE_CALL ip_version(AMX* amx, const cell* p) {
	return invoke<2>(amx, p, [](const Context& c) {
		auto* out = c.memory.cells(c.p[2]);
		if (!out) return Status::InvalidArgument;
		std::shared_ptr<const Database> database;
		const auto status = get_database(c, database);
		if (status == Status::Ok) *out = database->raw().metadata.ip_version;
		return status;
	});
}

cell AMX_NATIVE_CALL message(AMX* amx, const cell* p) {
	return invoke<4>(amx, p, [](const Context& c) {
		if (c.p[1] < 0 || c.p[1] > static_cast<cell>(Status::UnsupportedFamily)) return Status::InvalidArgument;
		return c.memory.output(c.p[2], c.p[3], c.p[4], status_message(static_cast<Status>(c.p[1])));
	});
}

const AMX_NATIVE_INFO natives[] = {
	{"mmdb_load", open_database<&Service::load>},
	{"mmdb_reload", open_database<&Service::reload>},
	{"mmdb_unload", unload}, {"mmdb_is_loaded", is_loaded}, {"mmdb_has_record", has_record},
	{"mmdb_get_string", get_string<false>}, {"mmdb_get_text", get_string<true>},
	{"mmdb_get_int", get_scalar<Scalar::Integer>}, {"mmdb_get_float", get_scalar<Scalar::Floating>},
	{"mmdb_get_bool", get_scalar<Scalar::Boolean>}, {"mmdb_get_type", get_scalar<Scalar::Type>},
	{"mmdb_get_size", get_scalar<Scalar::Size>}, {"mmdb_has_field", has_field},
	{"mmdb_get_prefix", prefix}, {"mmdb_get_database_type", metadata_string<false>},
	{"mmdb_get_build_epoch", metadata_string<true>}, {"mmdb_get_ip_version", ip_version},
	{"mmdb_get_status_message", message}, {nullptr, nullptr}
};

} // namespace

PawnBridge::PawnBridge(Service& service) : service_(service) {
	if (route) throw std::logic_error("Only one MMDB Pawn bridge may be active");
	route = this;
}

PawnBridge::~PawnBridge() {
	service_.clear();
	route = nullptr;
}

const AMX_NATIVE_INFO* native_table() noexcept { return natives; }
int native_count() noexcept { return static_cast<int>(std::size(natives) - 1); }

} // namespace mmdb
