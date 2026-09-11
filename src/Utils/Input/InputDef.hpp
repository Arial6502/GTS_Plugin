#pragma once

namespace GTS {

	// How the keybind page files a bind. Declaration order is the order the page lists them.
	// This is a user facing grouping, not a node one.
	enum class BindGroup : uint8_t {
		kCrush = 0,
		kGrab,
		kGrabPlay,
		kCleavage,
		kHugs,
		kThighCrush,
		kThighSandwich,
		kKickSwipe,
		kStomps,
		kVore,

		kMovement,
		kCamera,
		kAbilities,

		kInterface,
		kDebug,
		kOther,

		kTotal
	};

	[[nodiscard]] constexpr std::string_view BindGroupName(BindGroup a_Group) {
		switch (a_Group) {
			case BindGroup::kCrush:         { return "Butt/Breast Crush"; }
			case BindGroup::kGrab:          { return "Grab"; }
			case BindGroup::kGrabPlay:      { return "Grab Play"; }
			case BindGroup::kCleavage:      { return "Cleavage"; }
			case BindGroup::kHugs:          { return "Hugs"; }
			case BindGroup::kThighCrush:    { return "Thigh Crush"; }
			case BindGroup::kThighSandwich: { return "Thigh Sandwich"; }
			case BindGroup::kKickSwipe:     { return "Kicks/Swipes"; }
			case BindGroup::kStomps:        { return "Stomps"; }
			case BindGroup::kVore:          { return "Vore"; }
			case BindGroup::kMovement:      { return "Movement"; }
			case BindGroup::kCamera:        { return "Camera"; }
			case BindGroup::kAbilities:     { return "Abilities"; }
			case BindGroup::kInterface:     { return "Interface"; }
			case BindGroup::kDebug:         { return "Debug"; }
			default:                        { return "Other"; }
		}
	}

	enum class BindTriggerType : uint8_t {
		Once,
		Continuous,
		Release,
	};

	enum class BindBlockType : uint8_t {
		Automatic,
		Always,
		Never,
	};

	inline constexpr std::size_t MAX_BIND_KEYS = 5;

	// One keybind. The whole set lives in DefaultBinds; a node table only names the one it wants, and
	// which of its two tables the name appears in is what scopes the key.
	//
	// Keys are the built-in defaults. What the player actually bound comes from the keybind file and
	// overrides these; these are only what a fresh install and a reset fall back to.
	struct InputDef {

		std::string_view Name                            = "";
		std::array<std::string_view, MAX_BIND_KEYS> Keys = {};
		std::string_view UIName                          = "";
		std::string_view UIDescription                   = "";
		std::string_view Icon                            = "";
		BindTriggerType Trigger                          = BindTriggerType::Once;
		BindBlockType Block                              = BindBlockType::Automatic;
		BindGroup Group                                  = BindGroup::kOther;
		float Duration                                   = 0.0f;
		bool Exclusive                                   = false;
		bool Advanced                                    = false;

		[[nodiscard]] constexpr bool Empty() const {
			return Name.empty();
		}
	};

	//The live editable version of an InputDef.
	//Input.toml stores any diffs from the base InputDef by writing out this struct's data to it out.
	struct InputBind {
		std::string Name;
		std::vector<std::string> Keys;                  // DirectInput key names without the DIK_ prefix
		BindTriggerType Trigger = BindTriggerType::Once;
		BindBlockType Block = BindBlockType::Automatic;
		float Duration = 0.0f;
		bool Exclusive = false;
		bool Disabled = false;
	};
}
