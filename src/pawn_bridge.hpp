#pragma once

#include "core.hpp"
#include <amx/amx.h>
#include <unordered_map>

namespace maxora {
// The component owns this bridge; the C callback ABI uses one non-owning route.
// All calls, attachment, and detachment belong to the server thread.
class PawnBridge final {
public:
    explicit PawnBridge(Service& service);
    ~PawnBridge();
    PawnBridge(const PawnBridge&) = delete;
    PawnBridge& operator=(const PawnBridge&) = delete;
    Status attach(AMX* amx);
    void detach(AMX* amx) noexcept;
    void clear() noexcept;
    Service& service() noexcept { return service_; }
    Owner owner(AMX* amx) const noexcept;
private:
    Service& service_;
    std::unordered_map<AMX*, Owner> scripts_;
    Owner next_owner_ = 1;
};
const AMX_NATIVE_INFO* native_table() noexcept;
int native_count() noexcept;
} // namespace maxora
