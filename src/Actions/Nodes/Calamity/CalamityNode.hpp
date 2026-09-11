#pragma once

#include "Actions/Core/ActorStateMap.hpp"
#include "Actions/Core/IActionNode.hpp"

namespace GTS::Actions {

	class CalamityNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kCalamity; }
		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] ExitPolicy Exit() const override;
		[[nodiscard]] std::span<const EntryDef> Entries() const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;
		[[nodiscard]] std::span<const VanillaBlock> Blocks() const override;

		[[nodiscard]] bool CanEnter(const EntryContext& a_Ctx) const override;

		void OnEnter(const ActionContext& a_Ctx) override;
		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;

		void OnForget(RE::FormID a_Owner) override;
		void OnReset() override;

		[[nodiscard]] std::string_view StateName(RE::FormID a_Owner) const override;

		// Which of the two the giant is playing. Both assert GTS_isCastingShrink, so the graph cannot
		// tell them apart and neither can the machine. Only the log and the handlers care.
		enum class Kind : std::uint8_t {
			kShrink,
			kErase,
		};

		struct State {
			Kind Kind = Kind::kShrink;

			// The shrink slows its targets and gives the speed back at GTS_TC_RuneEnd. Tracked so an
			// exit before that point does not hand it back a second time.
			bool Slowed = false;
		};

		static State* Get(RE::FormID a_Owner);
		static State& GetOrAdd(RE::FormID a_Owner);

		private:
		static inline ActorStateMap<State> m_State = {};
	};
}
