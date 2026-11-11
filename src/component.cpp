#include "pawn_bridge.hpp"
#include <sdk.hpp>
#include <Server/Components/Pawn/pawn.hpp>

namespace {

class MMDBComponent final : public IComponent, public PawnEventHandler {
public:
	// Preserve the component UID so the server detects duplicate installations.
	PROVIDE_UID(0x89A5F19C36A63B4C);

	MMDBComponent() : bridge_(service_) {}
	~MMDBComponent() override { disconnect(); }
	StringView componentName() const override { return "MMDB"; }
	SemanticVersion componentVersion() const override {
		return {OMP_MMDB_VERSION_MAJOR, OMP_MMDB_VERSION_MINOR, OMP_MMDB_VERSION_PATCH, 0};
	}

	void onLoad(ICore* core) override {
		core_ = core;
		core_->printLn("MMDB " OMP_MMDB_VERSION ": local MMDB component loaded");
	}

	void onInit(IComponentList* list) override {
		pawn_ = list->queryComponent<IPawnComponent>();
		if (pawn_) pawn_->getEventDispatcher().addEventHandler(this);
		else core_->printLn("MMDB: Pawn component unavailable; Pawn API disabled");
	}

	void onFree(IComponent* component) override {
		if (component == pawn_) disconnect();
	}

	// open.mp can reset components after the new mode's init callback. Keep
	// live scripts intact and clean each namespace at the matching AMX unload.
	void reset() override {}
	void free() override { delete this; }

	void onAmxLoad(IPawnScript& script) override {
		auto* amx = script.GetAMX();
		try {
			if (bridge_.attach(amx) == mmdb::Status::Ok) {
				const auto error = script.Register(mmdb::native_table(), mmdb::native_count());
				// Other components may still have natives to register.
				if (error == AMX_ERR_NONE || error == AMX_ERR_NOTFOUND) return;
				bridge_.detach(amx);
			}
		} catch (...) {
			bridge_.detach(amx);
		}
		core_->printLn("MMDB: failed to attach Pawn script");
	}

	void onAmxUnload(IPawnScript& script) override { bridge_.detach(script.GetAMX()); }

private:
	void disconnect() noexcept {
		if (pawn_) pawn_->getEventDispatcher().removeEventHandler(this);
		pawn_ = nullptr;
		service_.clear();
	}

	ICore* core_ = nullptr;
	IPawnComponent* pawn_ = nullptr;
	mmdb::Service service_;
	mmdb::PawnBridge bridge_;
};

} // namespace

COMPONENT_ENTRY_POINT() {
	try {
		return new MMDBComponent();
	} catch (...) {
		return nullptr;
	}
}
