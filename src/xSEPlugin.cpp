#include "Version.hpp"
#include "Hooks/Hooks.hpp"
#include "Papyrus/Papyrus.hpp"
#include "Systems/Events/EventRegistry.hpp"
#include "Utils/Plugin/Logger.hpp"

namespace {

	void InitializePapyrus() {

		if (SKSE::GetPapyrusInterface()->Register(GTS::register_papyrus)) {
			logger::info("Papyrus functions bound");
			return;
		}
		GTS::ReportAndExit("Init: Could not register Papyrus bindings.");
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
	//SKSE Init
	{
		REL::Module::reset(); //Clib Init bug workaround.
		Init(a_skse);
	}

	//DLL Init
	{
		logger::Initialize();                                //Logger Init
		GTS::RegisterEventListeners();                       //Register All EventListeners
		GTS::EventDispatcher::Init(_byteswap_ulong('GTSP')); //Init EventDispatcher
		InitializePapyrus();                                 //Register Papyrus script bindings
		SKSE::GetTrampoline().create(384);                   //Don't forget to increase when you add new callhooks.
		Hooks::HookManager::InstallNormal();                 //Install Regular Hooks.
	}

	logger::info("SKSEPluginLoad OK");

	return true;
}

SKSEPluginInfo (
	.Version              = GTSPlugin::ModVersion,
	.Name                 = GTSPlugin::ModName,
	.Author               = {},
	.SupportEmail         = {},
	.StructCompatibility  = SKSE::StructCompatibility::Independent,
	.RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary,
	.MinimumSKSEVersion   = {},
)
