#pragma once

#include "Actions/Core/IActionNode.hpp"

// Butt crush, knee crush and boob crush are one family: one keybind, one cooldown, one growth
// mechanic and one temporary size record. They split into two nodes only because the graph gives
// the crawling variant a discriminator and the sneaking one none. Everything below is what both
// sides of that split share.
namespace GTS::Actions::Crush {

	// Started by ButtCrushController and keyed on the tiny, so both nodes cancel it the same way.
	inline constexpr std::string_view TaskAttachFmt = "ButtCrush_{}";

	
	[[nodiscard]] std::span<const std::string_view> BodyRumbleNodes();  // The breast set is the body set plus four breast bones.
	[[nodiscard]] std::span<const std::string_view> BreastRumbleNodes();
	[[nodiscard]] std::span<const std::string_view> ImpactBodyNodes(); 	// The seven bones both nodes damage on impact and over time.

	// Power is applied per node as given. Divide by the span size first where the total is meant to
	// be split across the body.
	void StartRumble(std::string_view a_Tag, RE::Actor& a_Actor, float a_Power, float a_Halflife, std::span<const std::string_view> a_Nodes);
	void StopRumble(std::string_view a_Tag, RE::Actor& a_Actor, std::span<const std::string_view> a_Nodes);

	void RecordStart(RE::Actor* a_Giant);

	// SetButtCrushSize(reset) restores the scale recorded at entry and clears the record. Calling it
	// a second time would find no record and clamp the actor to their natural scale, so the restore
	// only runs while one is actually outstanding.
	void RestoreSize(RE::Actor* a_Giant);

	// The attach task is keyed on the tiny, so only the node knows the names to cancel. Everything
	// else about letting a tiny go is Cleanup's.
	void CancelHeldTasks(RE::FormID a_Owner, PossessionSlot a_Slot);

	// Entry names are node specific, because Perform has to route them from idle with nothing else to
	// disambiguate. Stance is what picks the node, so every caller that starts a crush goes through
	// here rather than naming one of them. a_Suffix is "Enter" or "EnterQuick".
	[[nodiscard]] std::string EntryAction(RE::Actor* a_Actor, std::string_view a_Suffix);

	// a_WantCrawling separates the two nodes: the crawling stance belongs to boob crush and every
	// other stance to butt crush.
	[[nodiscard]] bool CanEnter(const EntryContext& a_Ctx, bool a_WantCrawling);

	// The cooldown half of entry, split out because it also reports why on a real press. Probing
	// keeps it silent.
	[[nodiscard]] bool CanStart(const EntryContext& a_Ctx);

	// a_Blocked is whatever the node itself rules out - a quick entry, or a drop already in flight.
	[[nodiscard]] bool CanGrow(const ActionContext& a_Ctx, bool a_Blocked);
	[[nodiscard]] bool VerifyGrow(const ActionContext& a_Ctx);
	[[nodiscard]] bool VerifyStart(const EntryContext& a_Ctx);

	// The half both growth handlers share: growth count, size record, the spring, stamina and the
	// growth sound. The moans and the rumble differ per node and stay with the caller.
	void ApplyGrowth(const ActionContext& a_Ctx, std::string_view a_SpringName, float a_NpcStaminaScale);
}
