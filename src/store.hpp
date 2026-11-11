#pragma once

#include "core.hpp"

#include <map>
#include <unordered_map>

namespace mmdb {

// Registry operations belong to the server thread. A lookup takes a local
// immutable snapshot that can outlive a replacement or an unload.
class Service final {
public:
	using Owner = const void*;
	static constexpr std::size_t max_databases_per_script = 64;
	static constexpr std::size_t max_databases = 4095;
	static constexpr std::size_t max_name = 63;

	Service() = default;
	Status attach(Owner owner);
	bool attached(Owner owner) const noexcept { return scripts_.contains(owner); }
	void detach(Owner owner) noexcept;
	void clear() noexcept;

	Status load(Owner owner, std::string_view name, std::string_view path);
	Status reload(Owner owner, std::string_view name, std::string_view path);
	Status unload(Owner owner, std::string_view name) noexcept;
	Status database(Owner owner, std::string_view name, std::shared_ptr<const Database>& out) const noexcept;
	Status lookup(Owner owner, std::string_view name, std::string_view ip, Lookup& out) const noexcept;
	std::size_t size() const noexcept { return size_; }

private:
	using Namespace = std::map<std::string, std::shared_ptr<const Database>, std::less<>>;

	static bool valid_name(std::string_view name) noexcept;
	std::unordered_map<Owner, Namespace> scripts_;
	std::size_t size_ = 0;
};

} // namespace mmdb
