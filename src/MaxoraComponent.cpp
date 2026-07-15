#include "MaxoraComponent.hpp"

MMDB_s MaxoraComponent::mmdb_;
bool MaxoraComponent::isLoaded_ = false;
std::string MaxoraComponent::lastError_;
IPawnComponent* MaxoraComponent::pawn_ = nullptr;

IPawnComponent* MaxoraComponent::GetPawnComponent()
{
	return pawn_;
}

bool MaxoraComponent::LoadDB(const char* filename)
{
	MMDB_s temp_mmdb;
	int status = MMDB_open(filename, MMDB_MODE_MMAP, &temp_mmdb);
	if (status != MMDB_SUCCESS)
	{
		lastError_ = MMDB_strerror(status);
		return false;
	}

	if (isLoaded_)
	{
		MMDB_close(&mmdb_);
	}

	mmdb_ = temp_mmdb;
	isLoaded_ = true;
	lastError_.clear();
	return true;
}

void MaxoraComponent::UnloadDB()
{
	if (isLoaded_)
	{
		MMDB_close(&mmdb_);
		isLoaded_ = false;
	}
}

bool MaxoraComponent::IsLoaded()
{
	return isLoaded_;
}

MMDB_s* MaxoraComponent::GetDB()
{
	return &mmdb_;
}

const std::string& MaxoraComponent::GetLastError()
{
	return lastError_;
}

void MaxoraComponent::SetLastError(const std::string& err)
{
	lastError_ = err;
}

void MaxoraComponent::onLoad(ICore* c)
{
	core_ = c;
	core_->printLn("Maxora Plugin (libmaxminddb) loading...");
}

void MaxoraComponent::onInit(IComponentList* components)
{
	pawn_ = components->queryComponent<IPawnComponent>();
	if (pawn_)
	{
		pawn_->getEventDispatcher().addEventHandler(this);
	}
}

void MaxoraComponent::onReady()
{
	if (core_)
	{
		core_->printLn("Maxora Plugin loaded successfully.");
	}
}

void MaxoraComponent::onFree(IComponent* component)
{
	if (component == pawn_ || component == this)
	{
		if (pawn_)
		{
			pawn_->getEventDispatcher().removeEventHandler(this);
			pawn_ = nullptr;
		}
	}
}

void MaxoraComponent::free()
{
	UnloadDB();
	delete this;
}

void MaxoraComponent::reset() {}

extern void RegisterNatives(IPawnScript& script);

void MaxoraComponent::onAmxLoad(IPawnScript& script)
{
	RegisterNatives(script);
}

void MaxoraComponent::onAmxUnload(IPawnScript& script) {}

COMPONENT_ENTRY_POINT()
{
	return new MaxoraComponent();
}
