#pragma once

#include "Actions/Core/ActorStateMap.hpp"
#include "Actions/Nodes/Crush/CrushCommon.hpp"

namespace GTS::Actions {

	// Standing butt crush and the sneaking knee crush. They are one node because the graph gives them
	// one signature: GTS_IsButtCrushing with no discriminator between them, so nothing could tell two
	// nodes apart. Their annotation sets are disjoint, so whichever set fires is the variant playing.
	//
	// The crawling variant is BoobCrushNode, which does have its own discriminator.
	class ButtCrushNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kButtCrush; }
		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] std::span<const std::string_view> ExitSignals() const override;
		[[nodiscard]] std::string_view AbortSignal() const override;
		[[nodiscard]] std::span<const EntryDef> Entries() const override;
		[[nodiscard]] std::span<const ActionDef> Actions() const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;

		[[nodiscard]] bool CanEnter(const EntryContext& a_Ctx) const override;

		void OnEnter(const ActionContext& a_Ctx) override;
		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;

		void OnForget(RE::FormID a_Owner) override;
		void OnReset() override;
		void RegisterInput() override;
		[[nodiscard]] bool StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view a_Action, bool a_Explain) const override;

		[[nodiscard]] std::string_view StateName(RE::FormID a_Owner) const override;

		// Which animation set the graph picked. Decided by stance at entry, used for logging and for
		// the cooldown message only - the handlers are per tag and do not consult it.
		enum class Variant : std::uint8_t {
			kStanding,
			kKnee,
		};

		struct State {
			Variant Variant = Variant::kStanding;

			// The quick entry has no held loop, so it offers neither growth nor the attack.
			bool Quick = false;

			// The fall-down has been asked for. Blocks a second request while it plays out.
			bool Dropping = false;
		};

		static State* Get(RE::FormID a_Owner);
		static State& GetOrAdd(RE::FormID a_Owner);

		private:
		static inline ActorStateMap<State> m_State = {};
	};
}
