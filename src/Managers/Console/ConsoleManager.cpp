#include "Managers/Console/ConsoleManager.hpp"

#include "Data/Persistent.hpp"
#include "Managers/AttackManager.hpp"
#include "Scale/Scale.hpp"
#include "Version.hpp"
#include "git.h"

namespace {

	using namespace GTS;

	//Cheap edit distance, only ever run when a command was not found.
	std::size_t EditDistance(std::string_view a_Lhs, std::string_view a_Rhs) {

		std::vector<std::size_t> Row(a_Rhs.size() + 1);
		std::iota(Row.begin(), Row.end(), std::size_t{ 0 });

		for (std::size_t i = 1; i <= a_Lhs.size(); ++i) {

			std::size_t Diagonal = Row[0];
			Row[0] = i;

			for (std::size_t j = 1; j <= a_Rhs.size(); ++j) {
				const std::size_t Previous = Row[j];
				Row[j] = std::min({ Row[j] + 1, Row[j - 1] + 1, Diagonal + (a_Lhs[i - 1] != a_Rhs[j - 1] ? 1u : 0u) });
				Diagonal = Previous;
			}
		}

		return Row.back();
	}

	std::string ArgCountText(std::uint32_t a_Count) {
		return std::format("{} argument{}", a_Count, a_Count == 1 ? "" : "s");
	}
}

namespace GTS {

	//----------------
	// Registration
	//----------------

	void ConsoleManager::RegisterCommand(ConsoleCommand a_Command) {

		if (a_Command.Name.empty()) {
			logger::warn("Refusing to register a console command with no name");
			return;
		}

		a_Command.Name = str_tolower(a_Command.Name);

		if (!a_Command.Callback) {
			logger::warn("Refusing to register console command \"{}\": no callback", a_Command.Name);
			return;
		}

		if (a_Command.MaxArgs < a_Command.MinArgs) {
			a_Command.MaxArgs = a_Command.MinArgs;
		}

		const std::string Name = a_Command.Name;

		for (const std::string& Alias : a_Command.Aliases) {
			RegisteredAliases.insert_or_assign(str_tolower(Alias), Name);
		}

		const auto [It, Inserted] = RegisteredCommands.insert_or_assign(Name, std::move(a_Command));

		if (!Inserted) {
			logger::warn("Console command \"{} {}\" was registered twice, the later one wins", kPrefix, Name);
		}

		logger::info("Registered Console Command \"{} {} {}\"", kPrefix, Name, It->second.Usage);
	}

	void ConsoleManager::RegisterCommand(std::string_view a_Name, const std::function<void()>& a_Callback, const std::string& a_Desc) {

		RegisterCommand(ConsoleCommand{
			.Name = std::string(a_Name),
			.Desc = a_Desc,
			.Callback = [a_Callback](const ConsoleArgs&) { a_Callback(); },
		});
	}

	const ConsoleCommand* ConsoleManager::Find(const std::string& a_Name) {

		if (const auto It = RegisteredCommands.find(a_Name); It != RegisteredCommands.end()) {
			return &It->second;
		}

		if (const auto It = RegisteredAliases.find(a_Name); It != RegisteredAliases.end()) {
			if (const auto Target = RegisteredCommands.find(It->second); Target != RegisteredCommands.end()) {
				return &Target->second;
			}
		}

		return nullptr;
	}

	//------------
	// Dispatch
	//------------

	bool ConsoleManager::Process(const std::string& a_Line) {

		if (RegisteredCommands.empty()) {
			return false;
		}

		const std::vector<std::string> Tokens = ConsoleArgs::Tokenize(a_Line);

		//No "gts" up front, so the line is not ours.
		if (Tokens.empty() || str_tolower(Tokens.front()) != kPrefix) {
			return false;
		}

		if (Tokens.size() < 2) {
			CMD_Help(ConsoleArgs("help", {}));
			return true;
		}

		const std::string Name = str_tolower(Tokens[1]);
		const ConsoleCommand* Command = Find(Name);

		if (!Command) {
			PrintUnknown(Name);
			return true;
		}

		Dispatch(*Command, ConsoleArgs(Command->Name, { Tokens.begin() + 2, Tokens.end() }));
		return true;
	}

	void ConsoleManager::Dispatch(const ConsoleCommand& a_Command, const ConsoleArgs& a_Args) {

		const auto Given = static_cast<std::uint32_t>(a_Args.Count());

		if (Given < a_Command.MinArgs) {
			Cprint("{} {} needs at least {}.", kPrefix, a_Command.Name, ArgCountText(a_Command.MinArgs));
			PrintUsage(a_Command);
			return;
		}

		if (Given > a_Command.MaxArgs) {

			if (a_Command.MaxArgs == 0) {
				Cprint("{} {} takes no arguments.", kPrefix, a_Command.Name);
			}
			else {
				Cprint("{} {} takes at most {}.", kPrefix, a_Command.Name, ArgCountText(a_Command.MaxArgs));
			}

			PrintUsage(a_Command);
			return;
		}

		// A command throwing would otherwise unwind through the console hook and take the game
		// with it.
		try {
			a_Command.Callback(a_Args);
		}
		catch (const std::exception& Error) {
			logger::error("Console command \"{} {}\" threw: {}", kPrefix, a_Command.Name, Error.what());
			Cprint("{} {} failed: {}", kPrefix, a_Command.Name, Error.what());
		}
	}

	void ConsoleManager::PrintUnknown(const std::string& a_Name) {

		const std::string* Closest = nullptr;
		std::size_t Best = 3;   //Anything further away is not a typo, it is a different word.

		for (const auto& Name : RegisteredCommands | std::views::keys) {
			if (const std::size_t Distance = EditDistance(a_Name, Name); Distance < Best) {
				Best = Distance;
				Closest = &Name;
			}
		}

		if (Closest) {
			Cprint("Unknown command \"{}\". Did you mean \"{} {}\"?", a_Name, kPrefix, *Closest);
			return;
		}

		Cprint("Unknown command \"{}\". Type \"{} help\" for a list.", a_Name, kPrefix);
	}

	void ConsoleManager::PrintUsage(const ConsoleCommand& a_Command) {

		if (a_Command.Usage.empty()) {
			Cprint("Usage: {} {}", kPrefix, a_Command.Name);
			return;
		}

		Cprint("Usage: {} {} {}", kPrefix, a_Command.Name, a_Command.Usage);
	}

	void ConsoleManager::PrintUsage(std::string_view a_Name) {

		if (const ConsoleCommand* Command = Find(str_tolower(std::string(a_Name)))) {
			PrintUsage(*Command);
		}
	}

	void ConsoleManager::OnSKSEDataLoaded() {
		Init();
	}

	//--------------------
	// Built in commands
	//--------------------

	void ConsoleManager::CMD_Help(const ConsoleArgs& a_Args) {

		//"gts help <command>" gives the long form for one command instead of the whole list.
		if (a_Args.Count() == 1) {

			const std::string Name = a_Args.Lower(0);
			const ConsoleCommand* Command = Find(Name);

			if (!Command) {
				PrintUnknown(Name);
				return;
			}

			Cprint("{} {} - {}", kPrefix, Command->Name, Command->Desc);
			PrintUsage(*Command);

			if (!Command->Aliases.empty()) {
				std::string Joined;
				for (const std::string& Alias : Command->Aliases) {
					Joined += Joined.empty() ? Alias : ", " + Alias;
				}
				Cprint("Aliases: {}", Joined);
			}

			return;
		}

		Cprint("--- List of available commands ---");

		for (const auto& [Name, Command] : RegisteredCommands) {
			if (Command.Usage.empty()) {
				Cprint("* {} {} - {}", kPrefix, Name, Command.Desc);
			}
			else {
				Cprint("* {} {} {} - {}", kPrefix, Name, Command.Usage, Command.Desc);
			}
		}

		Cprint("Type \"{} help <command>\" for details.", kPrefix);
	}

	void ConsoleManager::CMD_Version(const ConsoleArgs&) {
		Cprint("--- Giantess Mod: Size Matters ---");
		Cprint("Version: {}", GTSPlugin::ModVersion.string());
		Cprint("Dll Build Date: {} {}", __DATE__, __TIME__);
		Cprint("Git Commit Date: {}", git_CommitDate());
	}

	void ConsoleManager::CMD_Unlimited(const ConsoleArgs&) {

		Actor* Player = PlayerCharacter::GetSingleton();
		if (!Player) {
			return;
		}

		if (!Runtime::HasPerk(Player, Runtime::PERK.GTSPerkColossalGrowth)) {
			Cprint("You need to obtain the Colossal Growth perk to be able to use this command");
			return;
		}

		Persistent::UnlockMaxSizeSliders.value = !Persistent::UnlockMaxSizeSliders.value;
		Cprint("Max Size Sliders unlocked: {}", Persistent::UnlockMaxSizeSliders.value);
	}

	// gts scale <scale|reset> [target]
	void ConsoleManager::CMD_Scale(const ConsoleArgs& a_Args) {

		Actor* Target = a_Args.ResolveActor(1);

		if (!Target) {
			Cprint("\"{}\" is not an actor. Use player, target, or a form id.", a_Args.Raw(1));
			return;
		}

		//Says which branch was taken
		if (a_Args.Count() < 2 && !ConsoleArgs::SelectedActor()) {
			Cprint("Nothing selected in the console, using the player.");
		}

		if (!Persistent::GetActorData(Target)) {
			Cprint("{} is not tracked by the mod yet.", Target->GetDisplayFullName());
			return;
		}

		const float Natural = get_natural_scale(Target, true);
		const std::string Request = a_Args.Lower(0);

		float Wanted = 0.0f;

		if (Request == "reset" || Request == "natural") {
			Wanted = Natural;
		}
		else {

			const auto Parsed = a_Args.Float(0);

			if (!Parsed) {
				Cprint("\"{}\" is not a number.", a_Args.Raw(0));
				PrintUsage("scale");
				return;
			}

			if (*Parsed <= 0.0f || !std::isfinite(*Parsed)) {
				Cprint("Scale has to be a positive number.");
				return;
			}

			Wanted = *Parsed;
		}

		const float Before = get_target_scale(Target);

		set_target_scale(Target, Wanted);

		const float After = get_target_scale(Target);

		Cprint("{}: scale {:.2f} -> {:.2f}", Target->GetDisplayFullName(), Before, After);

		//set_target_scale clamps to the actor's max, so say so rather than look ignored.
		if (std::abs(After - Wanted) > 0.01f) {
			Cprint("Clamped to the max scale of {:.2f}. \"{} unlimited\" raises the cap.", get_max_scale(Target) * Natural, kPrefix);
		}
	}

	void ConsoleManager::Init() {

		logger::info("Loading Default Command List");

		RegisterCommand({
			.Name = "help",
			.Desc = "Show this list, or details for one command",
			.Usage = "[command]",
			.Aliases = { "?" },
			.MaxArgs = 1,
			.Callback = CMD_Help,
		});

		RegisterCommand({
			.Name = "version",
			.Desc = "Show plugin version",
			.Callback = CMD_Version,
		});

		RegisterCommand({
			.Name = "unlimited",
			.Desc = "Unlocks max size sliders",
			.Callback = CMD_Unlimited,
		});

		RegisterCommand({
			.Name = "scale",
			.Desc = "Set an actor's GTS target scale",
			.Usage = "<scale|reset> [player|target|formid]",
			.Aliases = { "setscale" },
			.MinArgs = 1,
			.MaxArgs = 2,
			.Callback = CMD_Scale,
		});
	}
}
