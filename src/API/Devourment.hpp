#pragma once

namespace GTS {

	// Matches the LOCUS_* constants documented at the top of Devourment's DevourmentManager.psc
	enum class DevourmentLocus : std::int32_t {
		kStomach = 0,
		kAnal = 1,
		kUnbirth = 2,
		kBreastLeft = 3,
		kBreastRight = 4,
		kCock = 5,
	};

	class Devourment final : public EventListener, public CInitSingleton<Devourment> {

		public:
		[[nodiscard]] static bool Enabled();

		// Registers the swallow with Devourment, bypassing its swallow chance and locus roll.
		// Returns false when Devourment cannot be reached at all, in which case the caller must
		// run the GTS path on the same frame.
		static bool Swallow(Actor* a_Pred, Actor* a_Prey, DevourmentLocus a_Locus);

		// True while Devourment has, or is still expected to have, this prey. Anything that would
		// otherwise consume the prey on the GTS side must check this per actor rather than
		// checking Enabled(), because a swallow Devourment refused stays with GTS.
		[[nodiscard]] static bool Owns(Actor* a_Prey);

		// Runs a_Fallback unless Devourment confirmed it took a_Prey. Call this from the kill
		// event of whatever action started the swallow.
		static void Resolve(Actor* a_Prey, const std::function<void()>& a_Fallback);

		void OnPluginReset() override;
		void OnGameActorReset(Actor* a_Actor) override;

		private:
		enum class SwallowState : std::uint8_t {
			kPending,
			kAccepted,
			kRejected,
		};

		struct SwallowRecord {
			SwallowState State = SwallowState::kPending;
			bool QueryInFlight = false;
		};

		static RE::TESQuest* GetManagerQuest();
		static void RequestVerify(RE::FormID a_Prey, Actor* a_PreyActor);

		static inline absl::flat_hash_map<RE::FormID, SwallowRecord> m_Swallows = {};
	};
}
