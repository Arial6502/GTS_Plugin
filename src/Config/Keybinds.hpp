#pragma once

#include "Config/Util/SettingsHandler.hpp"

namespace GTS {

    class Keybinds final : public CInitSingleton<Keybinds>, public SettingsHandler, public EventListener {

        private:
        static inline toml::basic_value<toml::ordered_type_config> TomlData;
        static inline std::filesystem::path InputFile = FileUtils::_basePath / "Input.toml";

        public:
        // Held alongside an action key to run that action on the nearest follower instead.
        static constexpr std::string_view TargetSwitchBind = "Action.TargetPlayer";

        // Held alongside an action's own keys to slow that animation instead of speeding it up.
        static constexpr std::string_view SlowDownBind = "Action.SlowDown";

        static inline std::vector<InputBind> InputEvents = {};

        // Bind names that start a state rather than being used while one runs. Drives the split the
        // settings page shows.
        static inline absl::flat_hash_set<std::string> StartsAction = {};

        // What each bind looks like untouched. Saving writes only the difference from this, so a
        // changed default reaches a player who never rebound it.
        static inline absl::flat_hash_map<std::string, InputBind> Defaults = {};
        static bool LoadKeybinds();

        // Whether a key starts a state or is something done while one runs. Only the registries know,
        // and they say so as they bind.
        static void NoteBindKind(std::string_view a_Name, bool a_StartsAction);
        static void ApplyStoredOverrides();

        // The bound keys as the active layout names them, joined for display. Empty when the bind
        // has no keys or does not exist.
        [[nodiscard]] static std::string ShortcutFor(std::string_view a_Name);
        static bool SaveKeybinds();
        static void ResetKeybinds();
        void OnSKSEDataLoaded() override;
    };
}