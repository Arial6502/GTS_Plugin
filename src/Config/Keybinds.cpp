#include "Config/Keybinds.hpp"
#include "Config/Util/KeybindHandler.hpp"
#include "Config/Util/FileUtils.hpp"
#include "Config/Config.hpp"
#include "Managers/Input/InputManager.hpp"

namespace GTS {

    bool Keybinds::LoadKeybinds() {

        std::lock_guard<std::mutex> lock(_ReadWriteLock);

        if (!FileUtils::CheckOrCreateFile(InputFile)) {
            return false;
        }

        const auto defaults = KeybindHandler::DefaultBindList();

        Defaults.clear();
        for (const auto& bind : defaults) {
            Defaults.emplace(bind.Name, bind);
        }

        try {
            InputEvents = defaults;
            TomlData = toml::parse<toml::ordered_type_config>(InputFile);
        }
        catch (const toml::exception& e) {
            logger::error("Toml load exception: {}", e.what());
            TomlData = toml::basic_value<toml::ordered_type_config>();
            return false;
        }
        catch (const std::exception& e) {
            logger::error("std toml load exception: {}", e.what());
            TomlData = toml::basic_value<toml::ordered_type_config>();
            return false;
        }
        catch (...) {
            logger::error("Unk toml load exception");
            return false;
        }

        KeybindHandler::ApplyOverrides(TomlData, InputEvents);
        return true;
    }

    void Keybinds::NoteBindKind(std::string_view a_Name, bool a_StartsAction) {
        if (a_StartsAction) {
            StartsAction.emplace(a_Name);
        }
    }

    std::string Keybinds::ShortcutFor(std::string_view a_Name) {

        const auto it = std::ranges::find_if(InputEvents, [&](const InputBind& a_e) {
            return a_e.Name == a_Name;
        });

        if (it == InputEvents.end()) {
            return {};
        }

        std::string out;

        for (const auto& key : it->Keys) {

            const auto parsed = InputKeyFromName(key);

            if (!out.empty()) {
                out += " + ";
            }

            out += parsed ? InputKeyDisplayName(*parsed, Config::UI.bLocalizedKeyNames) : key;
        }

        return out;
    }

    void Keybinds::ApplyStoredOverrides() {

        std::lock_guard<std::mutex> lock(_ReadWriteLock);

        KeybindHandler::ApplyOverrides(TomlData, InputEvents);
    }

    bool Keybinds::SaveKeybinds() {

        std::lock_guard<std::mutex> lock(_ReadWriteLock);

        if (!FileUtils::CheckOrCreateFile(InputFile)) {
            return false;
        }

        try {
            TomlData = KeybindHandler::BuildFile(InputEvents, Defaults);
        }
        catch (const toml::exception& e) {
            logger::error("TOML Exception when saving InputEvents: {}", e.what());
            return false;
        }
        catch (...) {
            logger::error("SaveKeybinds() -> Unknown Exception");
            return false;
        }

        return SaveTOMLToFile(TomlData, InputFile);
    }

    void Keybinds::ResetKeybinds() {

        for (auto& bind : InputEvents) {

            if (const auto it = Defaults.find(bind.Name); it != Defaults.end()) {
                bind = it->second;
            }
        }

        TomlData = toml::basic_value<toml::ordered_type_config>();

        SaveKeybinds();
        InputManager::GetSingleton().Init();
    }

    void Keybinds::OnSKSEDataLoaded() {
        Keybinds::LoadKeybinds();
    }
}
