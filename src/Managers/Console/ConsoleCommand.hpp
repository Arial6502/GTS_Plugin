#pragma once

namespace GTS {

	class ConsoleArgs {

		public:

		ConsoleArgs(std::string a_Command, std::vector<std::string> a_Tokens);

		[[nodiscard]] const std::string& Command() const { return m_Command; }
		[[nodiscard]] std::size_t Count() const { return m_Tokens.size(); }
		[[nodiscard]] bool Empty() const { return m_Tokens.empty(); }

		[[nodiscard]] std::string_view Raw(std::size_t a_Index) const;
		[[nodiscard]] std::string Lower(std::size_t a_Index) const;
		[[nodiscard]] std::string Rest(std::size_t a_From) const;

		[[nodiscard]] std::optional<float> Float(std::size_t a_Index) const;
		[[nodiscard]] std::optional<std::int32_t> Int(std::size_t a_Index) const;
		[[nodiscard]] std::optional<bool> Bool(std::size_t a_Index) const;

		//Hex, with or without an 0x prefix.
		[[nodiscard]] std::optional<RE::FormID> FormId(std::size_t a_Index) const;
		[[nodiscard]] bool HasFlag(std::string_view a_Flag) const;
		[[nodiscard]] std::size_t FirstValue(std::size_t a_From = 0) const;

		// "player"/"self"/"me", "target"/"sel", or a form id. When the index holds no token at
		// all this falls back to the console selection and then the player, which is what makes
		// "gts scale 5" work on whoever is clicked without naming them.
		[[nodiscard]] RE::Actor* ResolveActor(std::size_t a_Index) const;

		[[nodiscard]] static RE::Actor* SelectedActor();
		[[nodiscard]] static std::vector<std::string> Tokenize(std::string_view a_Line);

		private:

		std::string m_Command;
		std::vector<std::string> m_Tokens;
	};

	using ConsoleCommandFn = std::function<void(const ConsoleArgs&)>;

	inline constexpr std::uint32_t kUnlimitedArgs = std::numeric_limits<std::uint32_t>::max();

	struct ConsoleCommand {

		std::string Name;
		std::string Desc;
		std::string Usage;
		std::vector<std::string> Aliases;

		std::uint32_t MinArgs = 0;
		std::uint32_t MaxArgs = 0;

		ConsoleCommandFn Callback;
	};
}
