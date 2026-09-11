#include "Utils/Actor/RefPin.hpp"

#include "Data/Runtime.hpp"

namespace {

	using namespace GTS;

	using PromoteFn = bool (*)(void*, RE::TESObjectREFR*, RE::TESForm*);
	using DemoteFn = bool (*)(void*, RE::TESObjectREFR*, RE::TESForm*, bool);

	// Neither function reads its first argument, so the relocated address is passed straight through
	// rather than resolved as an object or as a pointer to one.
	void* Manager() {
		static const REL::Relocation<std::uintptr_t> Address{ REL::RelocationID(514177, 400326) };
		return reinterpret_cast<void*>(Address.address());
	}

	RE::TESForm* Owner() {
		return Runtime::GetQuest(Runtime::QUST.GTSQuestProgression);
	}
}

namespace GTS::RefPin {

	bool Pin(RE::TESObjectREFR* a_Ref) {

		auto* owner = Owner();

		if (!a_Ref || !owner) {
			return false;
		}

		if (Pinned(a_Ref)) {
			return true;
		}

		static const REL::Relocation<PromoteFn> Promote{ REL::RelocationID(15157, 15330) };

		// Refuses on its own for anything the game will not promote - the player, projectiles - by
		// way of the GetAllowPromoteToPersistent virtual.
		const bool promoted = Promote(Manager(), a_Ref, owner);

		logger::trace("RefPin: promote {:08X} -> {}", a_Ref->formID, promoted);
		return promoted;
	}

	void Unpin(RE::TESObjectREFR* a_Ref) {

		auto* owner = Owner();

		if (!a_Ref || !owner || !Pinned(a_Ref)) {
			return;
		}

		static const REL::Relocation<DemoteFn> Demote{ REL::RelocationID(15158, 15331) };

		Demote(Manager(), a_Ref, owner, false);
		logger::trace("RefPin: demote {:08X}", a_Ref->formID);
	}

	bool Pinned(const RE::TESObjectREFR* a_Ref) {

		auto* owner = Owner();

		if (!a_Ref || !owner) {
			return false;
		}

		const auto* promoted = a_Ref->extraList.GetByType<RE::ExtraPromotedRef>();

		if (!promoted) {
			return false;
		}

		for (const auto* form : promoted->promotedRefOwners) {
			if (form == owner) {
				return true;
			}
		}

		return false;
	}
}
