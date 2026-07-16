/**
 * @file main.cpp
 * @brief Entry point and component lifecycle management for the Maxora open.mp plugin.
 *
 * This file defines the main component class that integrates with the open.mp
 * server lifecycle. It is responsible for hooking into Pawn script loading/unloading
 * events to register native functions, and ensuring that any globally allocated
 * resources (like the MaxMind database) are properly freed when the server resets or closes.
 */

#include <sdk.hpp>
#include <Server/Components/Pawn/pawn.hpp>
#include <amx/amx.h>

#include "maxmind_store.hpp"
#include "natives.hpp"

// Global pointer to the pawn component. Native functions (in natives.cpp) will use this pointer to
// access the IPawnScript instance in order to read strings and references from the AMX machine.
IPawnComponent* gPawnComponent = nullptr;

/**
 * @class MaxoraComponent
 * @brief Manages the open.mp component integration and Pawn AMX lifecycle.
 *
 * Implements `IComponent` to hook into open.mp server events and `PawnEventHandler`
 * to hook into AMX script events (such as Gamemode/Filterscript load and unload).
 */
class MaxoraComponent final : public IComponent, public PawnEventHandler
{
  private:
	// Pointer to the open.mp Pawn component used to attach/detach event listeners.
	IPawnComponent* pawn_ = nullptr;

	// Flag to track whether the component has been fully initialized to prevent double-free issues.
	bool initialized_ = false;

  public:
	// Provide a unique 64-bit identifier for this open.mp component to prevent collisions with
	// other plugins.
	PROVIDE_UID(0x89A5F19C36A63B4C);

	/**
	 * @brief Component destructor. Ensures all resources are freed on destruction.
	 */
	~MaxoraComponent() override
	{
		cleanup();
	}

	/**
	 * @brief Returns the name of the component as it will appear in the server console.
	 */
	StringView componentName() const override
	{
		return "Maxora";
	}

	/**
	 * @brief Returns the semantic version of the component (Major, Minor, Patch, Tweak).
	 */
	SemanticVersion componentVersion() const override
	{
		return {1, 0, 0, 0};
	}

	/**
	 * @brief Called when the component is initially loaded by the server core.
	 */
	void onLoad(ICore* core) override
	{
		core->printLn("Maxora Plugin (libmaxminddb) loading...");
		initialized_ = true;
	}

	/**
	 * @brief Called when all components have been loaded. Used to resolve dependencies.
	 */
	void onInit(IComponentList* components) override
	{
		// Query the component list for the Pawn component instance.
		pawn_ = components->queryComponent<IPawnComponent>();
		if (!pawn_)
		{
			return; // If Pawn is not available (e.g. server runs without pawn scripts), we do
					// nothing.
		}

		// Store the instance globally so the native functions can read from the AMX.
		gPawnComponent = pawn_;

		// Register this class as an event listener for AMX events (onAmxLoad, onAmxUnload).
		pawn_->getEventDispatcher().addEventHandler(this);
	}

	/**
	 * @brief Called when the server is fully ready to process data.
	 */
	void onReady() override {}

	/**
	 * @brief Called when another component is being freed. Allows us to safely drop references.
	 */
	void onFree(IComponent* component) override
	{
		// If the Pawn component itself is being destroyed, we must unregister our event handler
		// and nullify our global pointer to avoid dangling references.
		if (component == pawn_ && pawn_)
		{
			pawn_->getEventDispatcher().removeEventHandler(this);
			pawn_ = nullptr;
			gPawnComponent = nullptr;
		}
	}

	/**
	 * @brief Called when the server is restarted (e.g. via the `gmx` RCON command).
	 */
	void reset() override
	{
		// On server reset, we must ensure the MaxMind database is unloaded to prevent memory leaks
		// or file locks.
		if (initialized_)
		{
			maxora::MaxmindStore::UnloadDB();
		}
	}

	/**
	 * @brief Called when the server drops this component completely.
	 */
	void free() override
	{
		cleanup();
		delete this; // Free the allocated instance
	}

	/**
	 * @brief Called every time a new Pawn script (Gamemode or Filterscript) is loaded into the AMX.
	 */
	void onAmxLoad(IPawnScript& script) override
	{
		// Register all our native functions (MMDB_Load, MMDB_GetString, etc.) into the newly loaded
		// script.
		if (initialized_)
		{
			maxora::RegisterNatives(script);
		}
	}

	/**
	 * @brief Called when a Pawn script is unloaded. We don't need to do anything specific here.
	 */
	void onAmxUnload(IPawnScript&) override {}

  private:
	/**
	 * @brief Internal helper to detach handlers and unload memory gracefully.
	 */
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
		maxora::MaxmindStore::UnloadDB(); // Force unload DB to free memory.
		initialized_ = false;
	}
};

// Suppress GCC/Clang warnings about 'cdecl' attribute being ignored on non-Windows platforms
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

/**
 * @brief Required entry point for open.mp plugins. The server core invokes this to get a new
 * instance.
 */
COMPONENT_ENTRY_POINT()
{
	return new MaxoraComponent();
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
