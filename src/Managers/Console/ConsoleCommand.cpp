#include "Managers/Console/ConsoleCommand.hpp"

namespace {

	//from_chars rejects a leading plus, which is the one thing people type that should work.
	[[nodiscard]] std::string_view StripPlus(std::string_view a_Text) {
		return (!a_Text.empty() && a_Text.front() == '+') ? a_Text.substr(1) : a_Text;
	}
}

namespace GTS {

	ConsoleArgs::ConsoleArgs(std::string a_Command, std::vector<std::string> a_Tokens)
		: m_Command(std::move(a_Command)), m_Tokens(std::move(a_Tokens)) {
	}

	std::string_view ConsoleArgs::Raw(std::size_t a_Index) const {
		return a_Index < m_Tokens.size() ? std::string_view(m_Tokens[a_Index]) : std::string_view{};
	}

	std::string ConsoleArgs::Lower(std::size_t a_Index) const {
		return str_tolower(std::string(Raw(a_Index)));
	}

	std::string ConsoleArgs::Rest(std::size_t a_From) const {

		std::string Joined;

		for (std::size_t i = a_From; i < m_Tokens.size(); ++i) {
			if (!Joined.empty()) {
				Joined += ' ';
			}
			Joined += m_Tokens[i];
		}

		return Joined;
	}

	std::optional<float> ConsoleArgs::Float(std::size_t a_Index) const {

		const std::string_view Text = StripPlus(Raw(a_Index));
		if (Text.empty()) {
			return std::nullopt;
		}

		float Value = 0.0f;
		const auto [End, Error] = std::from_chars(Text.data(), Text.data() + Text.size(), Value);

		//Trailing junk means the token was never a number, so "5x" is refused rather than read as 5.
		if (Error != std::errc{} || End != Text.data() + Text.size()) {
			return std::nullopt;
		}

		return Value;
	}

	std::optional<std::int32_t> ConsoleArgs::Int(std::size_t a_Index) const {

		const std::string_view Text = StripPlus(Raw(a_Index));
		if (Text.empty()) {
			return std::nullopt;
		}

		std::int32_t Value = 0;
		const auto [End, Error] = std::from_chars(Text.data(), Text.data() + Text.size(), Value);

		if (Error != std::errc{} || End != Text.data() + Text.size()) {
			return std::nullopt;
		}

		return Value;
	}

	std::optional<bool> ConsoleArgs::Bool(std::size_t a_Index) const {

		const std::string Text = Lower(a_Index);

		if (Text == "1" || Text == "on" || Text == "true" || Text == "yes" || Text == "enable") {
			return true;
		}

		if (Text == "0" || Text == "off" || Text == "false" || Text == "no" || Text == "disable") {
			return false;
		}

		return std::nullopt;
	}

	std::optional<RE::FormID> ConsoleArgs::FormId(std::size_t a_Index) const {

		std::string_view Text = Raw(a_Index);

		if (Text.starts_with("0x") || Text.starts_with("0X")) {
			Text = Text.substr(2);
		}

		if (Text.empty() || Text.size() > 8) {
			return std::nullopt;
		}

		std::uint32_t Value = 0;
		const auto [End, Error] = std::from_chars(Text.data(), Text.data() + Text.size(), Value, 16);

		if (Error != std::errc{} || End != Text.data() + Text.size()) {
			return std::nullopt;
		}

		return static_cast<RE::FormID>(Value);
	}

	bool ConsoleArgs::HasFlag(std::string_view a_Flag) const {

		return std::ranges::any_of(m_Tokens, [&](const std::string& a_Token) {
			return str_tolower(a_Token) == a_Flag;
		});
	}

	std::size_t ConsoleArgs::FirstValue(std::size_t a_From) const {

		for (std::size_t i = a_From; i < m_Tokens.size(); ++i) {
			if (!m_Tokens[i].starts_with('-')) {
				return i;
			}
		}

		return m_Tokens.size();
	}

	RE::Actor* ConsoleArgs::SelectedActor() {

		const RE::ObjectRefHandle Handle = RE::Console::GetSelectedRefHandle();
		if (!Handle) {
			return nullptr;
		}

		const RE::NiPointer<RE::TESObjectREFR> Pointer = Handle.get();
		if (!Pointer) {
			return nullptr;
		}

		RE::TESObjectREFR* Reference = Pointer.get();
		return Reference ? Reference->As<RE::Actor>() : nullptr;
	}

	RE::Actor* ConsoleArgs::ResolveActor(std::size_t a_Index) const {

		if (a_Index < m_Tokens.size()) {

			const std::string Token = Lower(a_Index);

			if (Token == "player" || Token == "self" || Token == "me") {
				return RE::PlayerCharacter::GetSingleton();
			}

			//Anything that is not an explicit request for the selection has to be a form id.
			if (Token != "target" && Token != "sel" && Token != "selected") {

				const auto Id = FormId(a_Index);
				return Id ? RE::TESForm::LookupByID<RE::Actor>(*Id) : nullptr;
			}
		}

		if (RE::Actor* Selected = SelectedActor()) {
			return Selected;
		}

		return RE::PlayerCharacter::GetSingleton();
	}

	std::vector<std::string> ConsoleArgs::Tokenize(std::string_view a_Line) {

		std::vector<std::string> Tokens;
		std::string Current;
		bool Quoted = false;

		const auto Flush = [&] {
			if (!Current.empty()) {
				Tokens.emplace_back(std::move(Current));
				Current.clear();
			}
		};

		for (const char Char : a_Line) {

			if (Char == '"') {
				//Closing a quote ends the token even when empty, so "" is a real argument.
				if (Quoted) {
					Tokens.emplace_back(std::move(Current));
					Current.clear();
				}
				Quoted = !Quoted;
				continue;
			}

			if (!Quoted && std::isspace(static_cast<unsigned char>(Char))) {
				Flush();
				continue;
			}

			Current += Char;
		}

		Flush();
		return Tokens;
	}
}
