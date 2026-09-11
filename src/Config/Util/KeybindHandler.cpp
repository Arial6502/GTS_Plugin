#include "Config/Util/KeybindHandler.hpp"
#include "Config/Settings/SettingsKeybinds.hpp"

namespace GTS {

	std::vector<InputBind> KeybindHandler::DefaultBindList() {

		std::vector<InputBind> result;
		result.reserve(DefaultBinds.size());

		for (const auto& def : DefaultBinds) {
			result.push_back(ToBind(def));
		}

		return result;
	}

	InputBind KeybindHandler::ToBind(const InputDef& a_Def) {

		InputBind entry{};

		entry.Name = std::string(a_Def.Name);
		entry.Exclusive = a_Def.Exclusive;
		entry.Trigger = a_Def.Trigger;
		entry.Duration = a_Def.Duration;
		entry.Block = a_Def.Block;

		for (const auto& key : a_Def.Keys) {
			if (!key.empty()) {
				entry.Keys.emplace_back(key);
			}
		}

		return entry;
	}

	const InputDef* KeybindHandler::Find(std::string_view a_Name) {

		const auto it = std::ranges::find_if(DefaultBinds, [&](const InputDef& a_Def) {
			return a_Def.Name == a_Name;
			});

		return it != DefaultBinds.end() ? &*it : nullptr;
	}

	namespace {

		constexpr std::array<std::string_view, 6> BindFields{ "keys", "trigger", "duration", "exclusive", "block", "disabled" };

		[[nodiscard]] bool LooksLikeBind(const toml::ordered_map<std::string, toml::basic_value<toml::ordered_type_config>>& a_table) {
			return std::ranges::any_of(BindFields, [&](std::string_view a_field) {
				return a_table.contains(std::string(a_field));
				});
		}
	}

	// a_name is the dotted path built from the table nesting, because a bind called
	// Ability.Size.GrowManual is written as [Ability.Size.GrowManual] and arrives here three levels
	// deep. The quoted flat form parses into a single key and reaches this the same way.
	void KeybindHandler::ReadSection(const toml::basic_value<toml::ordered_type_config>& a_value, std::string_view a_name,
		std::vector<InputBind>& a_inputEvents) {

		if (!a_value.is_table()) {
			return;
		}

		const auto& table = a_value.as_table();

		if (LooksLikeBind(table)) {

			auto bindIt = std::ranges::find_if(a_inputEvents, [&](const InputBind& a_ke) {
				return a_ke.Name == a_name;
				});

			if (bindIt != a_inputEvents.end()) {
				UpdateKeybindFromTable(*bindIt, table);
			}
		}

		// Not an else. One bind's name is another's prefix - GrowManual and GrowManual.Other - so a
		// table can carry a bind's own fields and still hold the sub-table of another bind.
		for (const auto& [name, value] : table) {

			if (!value.is_table()) {
				continue;
			}

			ReadSection(value, a_name.empty() ? name : std::string(a_name) + "." + name, a_inputEvents);
		}
	}

	void KeybindHandler::ApplyOverrides(const toml::basic_value<toml::ordered_type_config>& a_root, std::vector<InputBind>& a_inputEvents) {

		if (!a_root.is_table()) {
			return;
		}

		const auto& root = a_root.as_table();
		const auto versionIt = root.find("version");
		const std::int64_t version = versionIt != root.end() && versionIt->second.is_integer() ? versionIt->second.as_integer() : 0;

		if (version != FormatVersion) {

			if (!root.empty()) {
				logger::warn("Keybind file is version {}, this build reads {}. Keeping the defaults.", version, FormatVersion);
			}

			return;
		}

		for (const auto& [name, value] : root) {
			ReadSection(value, name, a_inputEvents);
		}
	}

	namespace {

		[[nodiscard]] bool SameAsDefault(const InputBind& a_bind, const InputBind& a_default) {
			return a_bind.Keys == a_default.Keys
				&& a_bind.Trigger == a_default.Trigger
				&& a_bind.Block == a_default.Block
				&& a_bind.Duration == a_default.Duration
				&& a_bind.Exclusive == a_default.Exclusive
				&& a_bind.Disabled == a_default.Disabled;
		}

		// Walks the dots, making a table for each step. Returns the table the bind's own fields go in,
		// which may already exist and hold another bind's sub-table.
		[[nodiscard]] toml::basic_value<toml::ordered_type_config>& TableFor(
			toml::basic_value<toml::ordered_type_config>& a_root, std::string_view a_name) {

			auto* node = &a_root;

			for (const auto part : std::views::split(a_name, '.')) {

				const std::string key(part.begin(), part.end());

				if (!node->contains(key) || !(*node)[key].is_table()) {

					(*node)[key] = toml::ordered_table{};

					// Implicit means the serializer prints no header for it, only for what is under
					// it. Without this every step of the path becomes an empty [Action], [Action.Grab]
					// of its own. The leaf turns itself back into a real table below.
					(*node)[key].as_table_fmt().fmt = toml::table_format::implicit;
				}

				node = &(*node)[key];
			}

			return *node;
		}
	}

	toml::basic_value<toml::ordered_type_config> KeybindHandler::BuildFile(const std::vector<InputBind>& a_inputEvents,
		const absl::flat_hash_map<std::string, InputBind>& a_defaults) {

		toml::basic_value<toml::ordered_type_config> root(toml::ordered_table{});
		root["version"] = FormatVersion;

		// By name, so a bind whose name is another's prefix is written first. Its own fields have to
		// land in the table before the sub-table does, or they end up inside it.
		std::vector<const InputBind*> changed;
		changed.reserve(a_inputEvents.size());

		for (const auto& bind : a_inputEvents) {

			const auto it = a_defaults.find(bind.Name);

			// Everything the user left alone stays out of the file, so a later change to a default
			// reaches them instead of being pinned by a copy of the old one.
			if (it != a_defaults.end() && SameAsDefault(bind, it->second)) {
				continue;
			}

			changed.push_back(&bind);
		}

		std::ranges::sort(changed, {}, &InputBind::Name);

		for (const auto* bind : changed) {

			// Every setting of a bind that changed, not only the setting that changed. A half written
			// bind would take the rest from whatever the defaults say next build.
			auto& entry = TableFor(root, bind->Name);

			// This one holds the bind, so it is printed. A parent that also happens to be a bind
			// gets its header back here rather than staying implicit.
			entry.as_table_fmt().fmt = toml::table_format::multiline;

			entry["keys"] = bind->Keys;
			entry["trigger"] = std::string(magic_enum::enum_name(bind->Trigger));
			entry["duration"] = bind->Duration;
			entry["exclusive"] = bind->Exclusive;
			entry["block"] = std::string(magic_enum::enum_name(bind->Block));
			entry["disabled"] = bind->Disabled;
		}

		return root;
	}

	void KeybindHandler::UpdateKeybindFromTable(InputBind& a_keybind,
		const toml::ordered_map<std::string, toml::basic_value<toml::ordered_type_config>>& a_tomlTable) {

		auto keysIt = a_tomlTable.find("keys");
		if (keysIt != a_tomlTable.end() && keysIt->second.is_array()) {
			std::vector<std::string> keys;
			for (const auto& keyVal : keysIt->second.as_array()) {
				if (keyVal.is_string()) {
					keys.push_back(keyVal.as_string());
				}
			}
			a_keybind.Keys = keys;
		}

		auto exclusiveIt = a_tomlTable.find("exclusive");
		if (exclusiveIt != a_tomlTable.end() && exclusiveIt->second.is_boolean()) {
			a_keybind.Exclusive = exclusiveIt->second.as_boolean();
		}

		auto triggerIt = a_tomlTable.find("trigger");
		if (triggerIt != a_tomlTable.end() && triggerIt->second.is_string()) {
			if (const auto trigger = magic_enum::enum_cast<BindTriggerType>(triggerIt->second.as_string())) {
				a_keybind.Trigger = *trigger;
			}
		}

		auto durationIt = a_tomlTable.find("duration");
		if (durationIt != a_tomlTable.end() &&
			(durationIt->second.is_floating() || durationIt->second.is_integer())) {
			if (durationIt->second.is_floating())
				a_keybind.Duration = static_cast<float>(durationIt->second.as_floating());
			else
				a_keybind.Duration = static_cast<float>(durationIt->second.as_integer());
		}

		auto blockIt = a_tomlTable.find("block");
		if (blockIt != a_tomlTable.end() && blockIt->second.is_string()) {
			if (const auto block = magic_enum::enum_cast<BindBlockType>(blockIt->second.as_string())) {
				a_keybind.Block = *block;
			}
		}

		auto disabledIt = a_tomlTable.find("disabled");
		if (disabledIt != a_tomlTable.end() && disabledIt->second.is_boolean()) {
			a_keybind.Disabled = disabledIt->second.as_boolean();
		}
	}
}
