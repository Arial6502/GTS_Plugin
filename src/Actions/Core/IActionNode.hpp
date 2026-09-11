#pragma once

#include "Actions/Core/ActionContext.hpp"

namespace GTS::Actions {

	struct VanillaBlock {
		std::string_view UserEvent;
		bool (*Handle)(RE::Actor*) = nullptr;
	};

	struct EntryDef {
		std::string_view Action;
		std::string_view Behaviour;

		bool (*Guard)(const EntryContext&) = nullptr;
		bool (*Verify)(const EntryContext&) = nullptr;
		std::span<const PossessionSlot> BlockedBy;
		std::string_view Input;
		bool Exits = false;
		std::span<const GraphExpect> Signature;
		std::string_view Confirm;
		void (*OnTaken)(const EntryContext&) = nullptr;
		std::string_view (*Resolve)(const EntryContext&) = nullptr;
	};

	struct ActionDef {
		std::string_view Action;
		std::string_view Behaviour;

		bool (*Guard)(const ActionContext&) = nullptr;
		bool (*Verify)(const ActionContext&) = nullptr;
		void (*OnTaken)(const ActionContext&) = nullptr;

		ActionId HandOff = ActionId::kNone;
		bool Exits = false;
		bool Aborts = false;

		std::span<const GraphExpect> Asserts;
		bool Kills = false;
		float Cooldown = 0.0f;
		std::string_view Input;

		// Same as EntryDef::Confirm, for an action that fills a slot on a node already running.
		std::string_view Confirm;
	};

	struct AnnotationDef {
		std::string_view Tag;
		void (*Handler)(const ActionContext&) = nullptr;
		bool Shared = false;
		bool ReleasesPartners = false;
		bool EndsIfPartnerGone = false;
	};

	class IActionNode {

		public:
		virtual ~IActionNode() = default;

		[[nodiscard]] virtual ActionId Id() const = 0;
		[[nodiscard]] virtual std::span<const GraphExpect> Signature() const = 0;
		[[nodiscard]] virtual ExitPolicy Exit() const                                      { return ExitPolicy::kAnnounced; }
		[[nodiscard]] virtual std::span<const std::string_view> ExitSignals() const        { return {}; }
		[[nodiscard]] virtual std::string_view AbortSignal() const                         { return {}; }
		[[nodiscard]] virtual std::span<const PossessionSlot> RequiredSlots() const        { return {}; }
		// What Leave releases. The hand and the breasts belong to the grab nodes, which override this.
		[[nodiscard]] virtual std::span<const PossessionSlot> OwnedSlots() const           { return kUncarriedSlots; }

		// Where the actors this node acts on are held. ReleasesPartners and partner deaths only look here.
		[[nodiscard]] virtual std::span<const PossessionSlot> PartnerSlots() const         { return OwnedSlots(); }

		// Another node's entry that may start while this one is running. This one steps aside for it
		// and is put back when it ends, so it must be a node that survives being left and re-entered:
		// a hold kept by the store does, an animation half way through does not.
		//
		// The grab uses it for the attacks the carry loop blends with. Everything else answers false,
		// and a request for one of those is refused as it always was.
		[[nodiscard]] virtual bool Permits(ActionId a_Id, RE::FormID a_Owner) const { return false; }

		// The node a living carried actor goes back to when nothing else is running. Only the grab.
		[[nodiscard]] virtual bool HoldsCarried() const { return false; }

		[[nodiscard]] virtual Liveness Alive(RE::FormID a_Owner, RE::Actor* a_Actor) const { return Liveness::kUnknown; }
		[[nodiscard]] virtual std::span<const EntryDef> Entries() const                    { return {}; }
		[[nodiscard]] virtual std::span<const ActionDef> Actions() const                   { return {}; }
		[[nodiscard]] virtual std::span<const AnnotationDef> Annotations() const           { return {}; }
		[[nodiscard]] virtual std::span<const AnnotationDef> PartnerAnnotations() const    { return {}; }
		[[nodiscard]] virtual bool CanEnter(const EntryContext& a_Ctx) const               { return true; }
		virtual void OnEnter(const ActionContext& a_Ctx)                                   {}
		virtual void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason)               {}
		virtual void OnUpdate(const ActionContext& a_Ctx, float a_Delta)                   {}
		virtual void OnForget(RE::FormID a_Owner)                                          {}
		virtual void OnReset()                                                             {}
		virtual void RegisterInput()                                                       {}
		[[nodiscard]] virtual std::span<const VanillaBlock> Blocks() const                 { return {}; }
		virtual bool StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view a_Action, bool a_Explain) const { return false; }
		[[nodiscard]] virtual std::string_view StateName(RE::FormID a_Owner) const         { return {}; }
	};
}
