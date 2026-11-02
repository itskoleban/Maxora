#include "pawn_bridge.hpp"
#include <sdk.hpp>
#include <Server/Components/Pawn/pawn.hpp>

namespace {
class MaxoraComponent final : public IComponent, public PawnEventHandler {
public:
    PROVIDE_UID(0x89A5F19C36A63B4C);
    MaxoraComponent() : bridge_(service_) {}
    ~MaxoraComponent() override { disconnect(); }
    StringView componentName() const override { return "Maxora"; }
    SemanticVersion componentVersion() const override { return {2, 0, 0, 0}; }
    void onLoad(ICore* core) override { core_ = core; core_->printLn("Maxora 2.0.0: local MMDB component loaded"); }
    void onInit(IComponentList* list) override {
        pawn_ = list->queryComponent<IPawnComponent>();
        if (pawn_) pawn_->getEventDispatcher().addEventHandler(this);
        else core_->printLn("Maxora: Pawn component unavailable; Pawn API disabled");
    }
    void onFree(IComponent* component) override {
        if (component == pawn_) disconnect();
    }
    // open.mp invokes component reset AFTER the new gamemode's init callback.
    // Per-script ownership handles cleanup at AMX unload; preserving live scripts
    // here also preserves filterscripts and resources created by the new mode.
    void reset() override {}
    void free() override { delete this; }
    void onAmxLoad(IPawnScript& script) override {
        try {
            if (bridge_.attach(script.GetAMX()) != maxora::Status::Ok) {
                core_->printLn("Maxora: cannot attach Pawn script");
                return;
            }
            // AMX_ERR_NOTFOUND may mean other components have not registered yet.
            script.Register(maxora::native_table(), maxora::native_count());
        } catch (...) { core_->printLn("Maxora: failed to attach Pawn script"); }
    }
    void onAmxUnload(IPawnScript& script) override { bridge_.detach(script.GetAMX()); }
private:
    void disconnect() noexcept {
        if (pawn_) pawn_->getEventDispatcher().removeEventHandler(this);
        pawn_ = nullptr;
        bridge_.clear();
        service_.clear();
    }
    ICore* core_ = nullptr;
    IPawnComponent* pawn_ = nullptr;
    maxora::Service service_;
    maxora::PawnBridge bridge_;
};
}
COMPONENT_ENTRY_POINT() {
    try { return new MaxoraComponent(); }
    catch (...) { return nullptr; }
}
