/**
 * @file main.cpp
 * @brief Entry point and component lifecycle management for the Maxora open.mp plugin.
 *
 * This file defines the main component class that integrates with the open.mp
 * server lifecycle and registers the Pawn event handlers and native functions.
 */

#include <sdk.hpp>
#include <Server/Components/Pawn/pawn.hpp>
#include <amx/amx.h>

#include "maxmind_store.hpp"
#include "natives.hpp"

// Global pointer to the pawn component. Native functions (in natives.cpp) will use this pointer to
// access the IPawnScript instance.
IPawnComponent* gPawnComponent = nullptr;

/**
 * @class MaxoraComponent
 * @brief Manages the open.mp component integration and Pawn AMX lifecycle.
 *
 * It is responsible for attaching natives to Pawn scripts when they load,
 * and cleaning up allocations when the server resets or unloads the plugin.
 */
class MaxoraComponent final : public IComponent, public PawnEventHandler
{
  private:
	// Pointer to the open.mp Pawn component used to hook AMX events
	IPawnComponent* pawn_ = nullptr;

	// Flag to track whether the component has been fully initialized
	bool initialized_ = false;

  public:
	// Provide a unique 64-bit identifier for this open.mp component
	PROVIDE_UID(0x89A5F19C36A63B4C);

	~MaxoraComponent() override
	{
		cleanup();
	}

	StringView componentName() const override
	{
		return "Maxora";
	}

	SemanticVersion componentVersion() const override
	{
		return {1, 0, 0, 0};
	}

	void onLoad(ICore* core) override
	{
		core->printLn("Maxora Plugin (libmaxminddb) loading...");
		initialized_ = true;
	}

	void onInit(IComponentList* components) override
	{
		pawn_ = components->queryComponent<IPawnComponent>();
		if (!pawn_)
		{
			return;
		}

		gPawnComponent = pawn_;
		pawn_->getEventDispatcher().addEventHandler(this);
	}

	void onReady() override {}

	void onFree(IComponent* component) override
	{
		if (component == pawn_ && pawn_)
		{
			pawn_->getEventDispatcher().removeEventHandler(this);
			pawn_ = nullptr;
			gPawnComponent = nullptr;
		}
	}

	void reset() override
	{
		if (initialized_)
		{
			maxora::MaxmindStore::UnloadDB();
		}
	}

	void free() override
	{
		cleanup();
		delete this;
	}

	void onAmxLoad(IPawnScript& script) override
	{
		if (initialized_)
		{
			maxora::RegisterNatives(script);
		}
	}

	void onAmxUnload(IPawnScript&) override {}

  private:
	void cleanup() noexcept
	{
		if (!initialized_)
		{
			return;
		}

		if (pawn_)
		{
			pawn_->getEventDispatcher().removeEventHandler(this);
			pawn_ = nullptr;
		}

		gPawnComponent = nullptr;
		maxora::MaxmindStore::UnloadDB();
		initialized_ = false;
	}
};

// Suppress GCC/Clang warnings about 'cdecl' attribute being ignored on non-Windows platforms
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

COMPONENT_ENTRY_POINT()
{
	return new MaxoraComponent();
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
