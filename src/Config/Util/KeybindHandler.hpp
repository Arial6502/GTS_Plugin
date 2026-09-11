#pragma once

namespace GTS {

	class KeybindHandler {

		public:
		// Every declared bind as an untouched InputBind, before the stored file is laid over it.
		[[nodiscard]] static std::vector<InputBind> DefaultBindList();

		// The serialized half of a declaration. What the file can change; the rest is fixed in code.
		[[nodiscard]] static InputBind ToBind(const InputDef& a_Def);

		// The declaration behind a bind name, or null for one nothing declares.
		[[nodiscard]] static const InputDef* Find(std::string_view a_Name);

		// Bumped only when the shape changes in a way an older reader cannot cope with. A file
		// carrying anything else is ignored and the defaults stand.
		static constexpr std::int64_t FormatVersion = 2;

		// Lays the stored file over the defaults. A name the list does not hold is left alone rather
		// than dropped: the action nodes register their keybinds after this first runs, so an
		// unknown name here is usually one that is about to exist.
		static void ApplyOverrides(const toml::basic_value<toml::ordered_type_config>& a_root, std::vector<InputBind>& a_inputEvents);

		// Every field of every bind is written so the serialized state is self-contained and does not
		// depend on the built-in defaults.
		[[nodiscard]] static toml::basic_value<toml::ordered_type_config> BuildFile(const std::vector<InputBind>& a_inputEvents, const absl::flat_hash_map<std::string, InputBind>& a_defaults);

		private:

		// A table is a keybind if it carries any field a keybind has; anything else is a section and
		// is walked into. That is what lets the sections be regrouped without breaking old files.
		static void ReadSection(const toml::basic_value<toml::ordered_type_config>& a_value, std::string_view a_name, std::vector<InputBind>& a_inputEvents);
		static void UpdateKeybindFromTable(InputBind& a_keybind, const toml::ordered_map<std::string, toml::basic_value<toml::ordered_type_config>>& a_tomlTable);
	};
}
