#pragma once

#include "store.hpp"
#include <amx/amx.h>

namespace mmdb {
// The component owns this bridge; the C callback ABI uses one non-owning route.
// All calls, attachment, and detachment belong to the server thread.
class PawnBridge final {
public:
	explicit PawnBridge(Service& service);
	~PawnBridge();
	PawnBridge(const PawnBridge&) = delete;
	PawnBridge& operator=(const PawnBridge&) = delete;
	Status attach(AMX* amx) { return service_.attach(amx); }
	void detach(AMX* amx) noexcept { service_.detach(amx); }
	Service& service() noexcept { return service_; }
private:
	Service& service_;
};
const AMX_NATIVE_INFO* native_table() noexcept;
int native_count() noexcept;
} // namespace mmdb
