#pragma once

namespace GTS::Actions {

	// One entry per animation state.
	#define GTS_ACTION_LIST(X) \
		X(ThighCrush)          \
		X(ThighSandwich)       \
		X(ThighSandwichButt)   \
		X(ButtCrush)           \
		X(BoobCrush)           \
		X(Stomp)               \
		X(Trample)             \
		X(KickSwipe)           \
		X(Vore)                \
		X(Hug)                 \
		X(Grab)                \
		X(GrabPlay)            \
		X(Cleavage)            \
		X(Growth)              \
		X(Shrink)              \
		X(Calamity)

	enum class ActionId : std::uint16_t {
		kNone = 0,
		
		#define GTS_ACTION_ENUM(a_Name) k##a_Name,
		GTS_ACTION_LIST(GTS_ACTION_ENUM)
		#undef GTS_ACTION_ENUM
	    
		kTotal,
	};

	constexpr std::size_t kActionCount = std::to_underlying(ActionId::kTotal);

	constexpr std::string_view ActionName(ActionId a_Id) {
		switch (a_Id) {
			case ActionId::kNone:
			{
				return "idle";
			}
			{
				#define GTS_ACTION_CASE(a_Name) case ActionId::k##a_Name: { return #a_Name; }
				GTS_ACTION_LIST(GTS_ACTION_CASE)
				#undef GTS_ACTION_CASE
			}
			default:
			{
				return "?";
			}
		}
	}

	enum class Liveness : std::uint8_t {
		kUnknown,
		kAlive,
		kOver,
	};

	enum class Stance : std::uint8_t {
		kStanding,
		kSneak,
		kCrawl,
	};

	constexpr std::string_view StanceName(Stance a_Stance) {
		switch (a_Stance) {
			case Stance::kSneak: { return "sneak"; }
			case Stance::kCrawl: { return "crawl"; }
			default:             { return "standing"; }
		}
	}

	enum class PossessionSlot : std::uint8_t {
		kHand,
		kArms,
		kBreasts,
		kButt,
		kThighs,
		kMouth,
		kTotal,
	};

	constexpr std::size_t kSlotCount = std::to_underlying(PossessionSlot::kTotal);

	enum class DeathIntent : std::uint8_t {
		kUnexpected,
		kIntended,
	};

	enum class DeathOutcome : std::uint8_t {
		kNone,
		kIntended,
		kUnexpected,
	};

	constexpr std::string_view DeathOutcomeName(DeathOutcome a_Outcome) {
		switch (a_Outcome) {
			case DeathOutcome::kIntended:   { return "intended"; }
			case DeathOutcome::kUnexpected: { return "unexpected"; }
			default:                        { return "none"; }
		}
	}

	constexpr bool IsCarriedSlot(PossessionSlot a_Slot) {
		return a_Slot == PossessionSlot::kHand || a_Slot == PossessionSlot::kBreasts;
	}

	inline constexpr std::array<PossessionSlot, kSlotCount> kAllSlots = {
		PossessionSlot::kHand,
		PossessionSlot::kArms,
		PossessionSlot::kBreasts,
		PossessionSlot::kButt,
		PossessionSlot::kThighs,
		PossessionSlot::kMouth,
	};

	inline constexpr PossessionSlot kUncarriedSlots[] = {
		PossessionSlot::kArms,
		PossessionSlot::kButt,
		PossessionSlot::kThighs,
		PossessionSlot::kMouth,
	};

	inline constexpr PossessionSlot kBlockedByHand[] = { PossessionSlot::kHand };

	constexpr std::string_view SlotName(PossessionSlot a_Slot) {
		switch (a_Slot) {
			case PossessionSlot::kHand:    { return "hand"; }
			case PossessionSlot::kArms:    { return "arms"; }
			case PossessionSlot::kBreasts: { return "breasts"; }
			case PossessionSlot::kButt:    { return "butt"; }
			case PossessionSlot::kThighs:  { return "thighs"; }
			case PossessionSlot::kMouth:   { return "mouth"; }
			default:                       { return "?"; }
		}
	}

	enum class ExitPolicy : std::uint8_t {
		kAnnounced,
		kUnannounced,
	};

	enum class ExitReason : std::uint8_t {
		kCompleted,
		kHandOff,

		// Stepped aside so something else could run, and put back when that finishes. A giant carrying
		// someone can still stomp, and the carry is not over while she does.
		kSuspended,

		kAborted,
		kDesync,
		kActorLost,
		kUnconfirmed,
	};

	enum class RequestResult : std::uint8_t {
		kAccepted,
		kUnknownAction,
		kNotCurrent,
		kAlreadyPending,
		kGuardFailed,
		kVerifyFailed,
		kAlreadyRunning,
		kOnCooldown,
		kGraphRefused,
	};

	constexpr std::string_view ExitReasonName(ExitReason a_Reason) {
		switch (a_Reason) {
			case ExitReason::kCompleted: { return "completed"; }
			case ExitReason::kHandOff:   { return "handoff"; }
			case ExitReason::kSuspended: { return "suspended"; }
			case ExitReason::kAborted:   { return "aborted"; }
			case ExitReason::kDesync:    { return "desync"; }
			case ExitReason::kActorLost: { return "actor lost"; }
			case ExitReason::kUnconfirmed:{ return "never confirmed"; }
			default:                     { return "?"; }
		}
	}

	// The node is stepping aside rather than finishing: another branch is taking over, or it is being
	// put back once whatever interrupted it is done. Either way its actors and its state stay as they
	// are, and nothing about the hold is undone.
	constexpr bool KeepsState(ExitReason a_Reason) {
		return a_Reason == ExitReason::kHandOff || a_Reason == ExitReason::kSuspended;
	}

	constexpr std::string_view RequestResultName(RequestResult a_Result) {
		switch (a_Result) {
			case RequestResult::kAccepted:      { return "accepted"; }
			case RequestResult::kUnknownAction: { return "no such action"; }
			case RequestResult::kNotCurrent:    { return "not the active node"; }
			case RequestResult::kAlreadyPending:{ return "already waiting on an entry"; }
			case RequestResult::kGuardFailed:   { return "guard failed"; }
			case RequestResult::kVerifyFailed:  { return "refused, told the player"; }
			case RequestResult::kAlreadyRunning:{ return "already running"; }
			case RequestResult::kOnCooldown:    { return "on cooldown"; }
			case RequestResult::kGraphRefused:  { return "graph refused behaviour"; }
			default:                            { return "?"; }
		}
	}

	struct AvailableAction {
		std::string_view Action;        // pass straight to Perform.
		std::string_view Input;         // keybind name.
		std::string_view UIName;
		std::string_view UIDescription;
		std::string_view Icon;
		bool Ready = false;             // guard passed.
	};
}
