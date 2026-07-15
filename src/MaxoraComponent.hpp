#pragma once

#include <sdk.hpp>
#include <core.hpp>
#include <Server/Components/Pawn/pawn.hpp>
#include <maxminddb.h>
#include <string>

class MaxoraComponent final : public IComponent, public PawnEventHandler
{
  public:
	PROVIDE_UID(0x89A5F19C36A63B4C); // Random unique ID

	StringView componentName() const override
	{
		return "Maxora";
	}
	SemanticVersion componentVersion() const override
	{
		return SemanticVersion(1, 0, 0, 0);
	}

	void onLoad(ICore* c) override;
	void onInit(IComponentList* components) override;
	void onReady() override;
	void onFree(IComponent* component) override;
	void free() override;
	void reset() override;

	// PawnEventHandler
	void onAmxLoad(IPawnScript& script) override;
	void onAmxUnload(IPawnScript& script) override;

	// Static Database Methods
	static bool LoadDB(const char* filename);
	static void UnloadDB();
	static bool IsLoaded();
	static MMDB_s* GetDB();
	static const std::string& GetLastError();
	static void SetLastError(const std::string& err);

	static IPawnComponent* GetPawnComponent();

  private:
	ICore* core_ = nullptr;
	static IPawnComponent* pawn_;

	static MMDB_s mmdb_;
	static bool isLoaded_;
	static std::string lastError_;
};
