#include "store.hpp"

#include <algorithm>
#include <utility>

namespace mmdb {

bool Service::valid_name(std::string_view name) noexcept {
	return !name.empty() && name.size() <= max_name && std::all_of(name.begin(), name.end(), [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
			c == '_' || c == '-' || c == '.';
	});
}

Status Service::attach(Owner owner) {
	if (!owner) return Status::InvalidArgument;
	scripts_.try_emplace(owner);
	return Status::Ok;
}

void Service::detach(Owner owner) noexcept {
	const auto script = scripts_.find(owner);
	if (script == scripts_.end()) return;
	size_ -= script->second.size();
	scripts_.erase(script);
}

void Service::clear() noexcept {
	scripts_.clear();
	size_ = 0;
}

Status Service::load(Owner owner, std::string_view name, std::string_view path) {
	if (!valid_name(name)) return Status::InvalidArgument;
	const auto script = scripts_.find(owner);
	if (script == scripts_.end()) return Status::InvalidArgument;
	auto& databases = script->second;
	if (databases.contains(name)) return Status::AlreadyLoaded;
	if (databases.size() == max_databases_per_script || size_ == max_databases) return Status::ResourceLimit;
	std::shared_ptr<const Database> database;
	const auto status = Database::open(path, database);
	if (status != Status::Ok) return status;
	databases.emplace(std::string(name), std::move(database));
	++size_;
	return Status::Ok;
}

Status Service::reload(Owner owner, std::string_view name, std::string_view path) {
	if (!valid_name(name)) return Status::InvalidArgument;
	const auto script = scripts_.find(owner);
	if (script == scripts_.end()) return Status::InvalidArgument;
	const auto entry = script->second.find(name);
	if (entry == script->second.end()) return Status::NotLoaded;
	return Database::open(path, entry->second);
}

Status Service::unload(Owner owner, std::string_view name) noexcept {
	if (!valid_name(name)) return Status::InvalidArgument;
	const auto script = scripts_.find(owner);
	if (script == scripts_.end()) return Status::InvalidArgument;
	const auto entry = script->second.find(name);
	if (entry == script->second.end()) return Status::NotLoaded;
	script->second.erase(entry);
	--size_;
	return Status::Ok;
}

Status Service::database(Owner owner, std::string_view name, std::shared_ptr<const Database>& out) const noexcept {
	if (!valid_name(name)) return Status::InvalidArgument;
	const auto script = scripts_.find(owner);
	if (script == scripts_.end()) return Status::InvalidArgument;
	const auto entry = script->second.find(name);
	if (entry == script->second.end()) return Status::NotLoaded;
	out = entry->second;
	return Status::Ok;
}

Status Service::lookup(Owner owner, std::string_view name, std::string_view ip, Lookup& out) const noexcept {
	std::shared_ptr<const Database> snapshot;
	const auto status = database(owner, name, snapshot);
	return status == Status::Ok ? Lookup::find(std::move(snapshot), ip, out) : status;
}

} // namespace mmdb
