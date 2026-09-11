#pragma once

#include "Actions/Core/ActorStateMap.hpp"
#include "Actions/Core/IActionNode.hpp"

namespace GTS::Actions {

	// Standing, sneaking and crawling vore. One node, because all three are one action reached by one
	// trigger, and the graph picks the animation set from the stance. Their annotation sets are
	// disjoint, so whichever set fires is the variant playing.
	//
	// Unlike the crush family this node holds several tinies at once. How many is decided before entry
	// by VoreController::GetVoreTargetsInFront, which caps the list on the giant's perks.
	class VoreNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kVore; }
		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] std::span<const std::string_view> ExitSignals() const override;
		[[nodiscard]] std::string_view AbortSignal() const override;
		[[nodiscard]] std::span<const EntryDef> Entries() const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;

		[[nodiscard]] bool CanEnter(const EntryContext& a_Ctx) const override;

		void OnEnter(const ActionContext& a_Ctx) override;
		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;

		void OnForget(RE::FormID a_Owner) override;
		void OnReset() override;
		void RegisterInput() override;
		[[nodiscard]] bool StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view a_Action, bool a_Explain) const override;

		[[nodiscard]] std::string_view StateName(RE::FormID a_Owner) const override;

		// Which animation set the graph picked. Set from the stance at entry and used for the log.
		enum class Variant : std::uint8_t {
			kStanding,
			kSneak,
			kCrawl,
		};

		enum class Stage : std::uint8_t {
			kReaching,
			kHeld,
			kSwallowed,
		};

		struct State {
			Variant Variant = Variant::kStanding;
			Stage Stage = Stage::kReaching;

			// The player took the free camera on entry and it has to be handed back on exit.
			bool FreeCamera = false;
		};

		static State* Get(RE::FormID a_Owner);
		static State& GetOrAdd(RE::FormID a_Owner);

		private:
		static inline ActorStateMap<State> m_State = {};
	};
}
