#pragma once

#include "Actions/Core/ActorStateMap.hpp"
#include "Actions/Core/IActionNode.hpp"

namespace GTS::Actions {

	class StompNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kStomp; }
		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] std::span<const std::string_view> ExitSignals() const override;
		[[nodiscard]] std::span<const EntryDef> Entries() const override;

		// Aims at the chosen actor rather than binding it: this node has no possession slot to put
		// one in, the aim search decides.
		[[nodiscard]] bool StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view a_Action, bool a_Explain) const override;
		[[nodiscard]] std::span<const ActionDef> Actions() const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;

		[[nodiscard]] Liveness Alive(RE::FormID a_Owner, RE::Actor* a_Actor) const override;

		void OnEnter(const ActionContext& a_Ctx) override;
		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;

		void OnForget(RE::FormID a_Owner) override;
		void OnReset() override;

		[[nodiscard]] std::string_view StateName(RE::FormID a_Owner) const override;

		// The stance the node entered in, which is how the graph picks the clip set. Only ever read
		// for the name in the action log.
		enum class Variant : std::uint8_t {
			kStanding,
			kSneak,
			kCrawl,
			kCrawlStrong,
		};

		struct State {
			Variant Variant = Variant::kStanding;

			// The graph asserts nothing for this variant, so the node answers for itself until its own
			// last annotation arrives.
			bool Silent = false;

			// GTSstomp_FootGrind*_MV_S fires once per rotation. The seventh is where Sonderbain's clip
			// leaves the tiny attached a second too long, which the count is here to catch.
			std::uint8_t GrindRotations = 0;
		};

		static State* Get(RE::FormID a_Owner);
		static State& GetOrAdd(RE::FormID a_Owner);

		private:
		static inline ActorStateMap<State> m_State = {};
	};
}
