#include "API/Devourment.hpp"

#include "Config/Config.hpp"
#include "Utils/PapyrusUtils.hpp"

namespace {

	// How long to keep asking Devourment whether it took the prey.
	constexpr double VerifyWindow = 2.0;
	std::mutex StateLock = {};
}

namespace GTS {

	bool Devourment::Enabled() {
		return Config::General.bDevourmentCompat && Runtime::IsDevourmentInstalled();
	}

	RE::TESQuest* Devourment::GetManagerQuest() {
		if (!Enabled()) {
			return nullptr;
		}
		return Runtime::GetQuest(Runtime::QUST.DevourmentManager);
	}

	bool Devourment::Swallow(Actor* a_Pred, Actor* a_Prey, DevourmentLocus a_Locus) {

		if (!a_Pred || !a_Prey) {
			return false;
		}

		auto* Quest = GetManagerQuest();
		if (!Quest) {
			logger::debug("Devourment: manager quest is unavailable, falling back to GTS vore");
			return false;
		}

		// A quest with a script attached is bound at load, so a null object means Devourment's
		// manager is not running and nothing we dispatch would arrive.
		if (!GetVMObjectPtr(Quest, "DevourmentManager", false)) {
			logger::debug("Devourment: DevourmentManager is not bound, falling back to GTS vore");
			return false;
		}

		bool DoEndo = false;
		{
			const auto& AllowEndo = Config::Gameplay.ActionSettings.bDVDoEndoOnTeam;
			if (AllowEndo && (IsTeammate(a_Pred) || a_Pred->IsPlayerRef()) && (IsTeammate(a_Prey) || a_Prey->IsPlayerRef())) {
				DoEndo = true;
			}
		}

		// The VM matches argument types exactly, it does not upcast. Arguments are packed from
		// the static C++ type, so these have to be cast to whatever the Papyrus signature
		// declares: RegisterDigestion and DisableEscape take Form, not Actor.
		auto* PredForm = static_cast<RE::TESForm*>(a_Pred);
		auto* PreyForm = static_cast<RE::TESForm*>(a_Prey);

		CallVMFunctionOn(Quest, "DevourmentManager", "RegisterDigestion", PredForm, PreyForm, DoEndo, static_cast<std::int32_t>(a_Locus));
		CallVMFunctionOn(Quest, "DevourmentManager", "DisableEscape", PreyForm);

		{
			std::unique_lock Lock(StateLock);
			m_Swallows[a_Prey->formID] = SwallowRecord{};
		}

		logger::debug("Devourment: registered {} in {} at locus {}", a_Prey->GetDisplayFullName(), a_Pred->GetDisplayFullName(), static_cast<std::int32_t>(a_Locus));
		return true;
	}

	void Devourment::RequestVerify(RE::FormID a_Prey, Actor* a_PreyActor) {

		auto* Quest = GetManagerQuest();

		if (!Quest || !a_PreyActor) {
			std::unique_lock Lock(StateLock);
			if (const auto Entry = m_Swallows.find(a_Prey); Entry != m_Swallows.end()) {
				Entry->second.QueryInFlight = false;
			}
			return;
		}

		std::function<void(std::optional<bool>)> Callback = [a_Prey](const std::optional<bool> a_IsPrey) {
			std::unique_lock Lock(StateLock);

			const auto Entry = m_Swallows.find(a_Prey);
			if (Entry == m_Swallows.end()) {
				return;
			}

			Entry->second.QueryInFlight = false;

			if (!a_IsPrey.has_value()) { // The call errored out.
				return;
			}

			if (*a_IsPrey) {
				Entry->second.State = SwallowState::kAccepted;
			}
		};

		// IsPrey declares ObjectReference, so an Actor-typed argument is refused
		CallVMFunctionOnReturn<bool>(Quest, "DevourmentManager", "IsPrey", Callback, static_cast<RE::TESObjectREFR*>(a_PreyActor));
	}

	bool Devourment::Owns(Actor* a_Prey) {

		if (!a_Prey) {
			return false;
		}

		std::unique_lock Lock(StateLock);
		const auto Entry = m_Swallows.find(a_Prey->formID);
		return Entry != m_Swallows.end() && Entry->second.State != SwallowState::kRejected;
	}

	void Devourment::Resolve(Actor* a_Prey, const std::function<void()>& a_Fallback) {

		if (!a_Prey) {
			return;
		}

		const RE::FormID PreyID = a_Prey->formID;
		const ActorHandle PreyHandle = a_Prey->GetHandle();

		{
			std::unique_lock Lock(StateLock);
			const auto Entry = m_Swallows.find(PreyID);

			if (Entry == m_Swallows.end()) { // Nothing was ever handed over, so this is ours to finish
				Lock.unlock();
				a_Fallback();
				return;
			}

			if (Entry->second.State == SwallowState::kAccepted) {
				m_Swallows.erase(Entry);
				return;
			}
		}

		const std::string Name = std::format("DVSwallow_{:X}", PreyID);

		TaskManager::Run(Name, [PreyID, PreyHandle, a_Fallback](const TaskUpdate& a_Update) {

			Actor* Prey = PreyHandle ? PreyHandle.get().get() : nullptr;

			// RegisterDigestion ends in DisappearPreyBy, so a prey Devourment accepted stops
			// being visible.
			bool Accepted = IsInvisible_Devourment(Prey);

			if (!Accepted) {
				std::unique_lock Lock(StateLock);
				const auto Entry = m_Swallows.find(PreyID);
				if (Entry == m_Swallows.end()) {
					return false;
				}

				Accepted = Entry->second.State == SwallowState::kAccepted;

				if (!Accepted && a_Update.runtime < VerifyWindow) { // Ask as well, in case it is held somewhere still visible
					if (!Entry->second.QueryInFlight) {
						Entry->second.QueryInFlight = true;
						Lock.unlock();
						RequestVerify(PreyID, Prey);
					}
					return true;
				}
			}

			{
				std::unique_lock Lock(StateLock);
				m_Swallows.erase(PreyID);
			}

			if (Accepted) {
				return false;
			}

			logger::info("Devourment did not take prey {:X}, running GTS vore instead", PreyID);
			a_Fallback();
			return false;
		});
	}

	void Devourment::OnPluginReset() {
		std::unique_lock Lock(StateLock);
		m_Swallows.clear();
	}

	void Devourment::OnGameActorReset(Actor* a_Actor) {
		if (a_Actor) {
			std::unique_lock Lock(StateLock);
			m_Swallows.erase(a_Actor->formID);
		}
	}
}
