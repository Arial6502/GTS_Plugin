#pragma once

#include "Managers/Console/ConsoleCommand.hpp"

namespace GTS {

	class ConsoleManager : public EventListener, public CInitSingleton<ConsoleManager> {

		public:

		static void Init();

		//Full form. Anything with arguments wants this one.
		static void RegisterCommand(ConsoleCommand a_Command);

		//Convenience for a command that takes no arguments.
		static void RegisterCommand(std::string_view a_Name, const std::function<void()>& a_Callback, const std::string& a_Desc);

		//True when the line was ours and has been handled, which stops the game compiling it.
		static bool Process(const std::string& a_Line);

		[[nodiscard]] static const std::string& Prefix() { return kPrefix; }

		//Prints "Usage: gts <name> <spec>". Commands can call this on a bad argument.
		static void PrintUsage(const ConsoleCommand& a_Command);
		static void PrintUsage(std::string_view a_Name);

		void OnSKSEDataLoaded() override;

		private:

		inline static const std::string kPrefix = "gts";

		//Ordered, so help always prints the same list in the same order.
		static inline std::map<std::string, ConsoleCommand> RegisteredCommands = {};

		//Alias to canonical name.
		static inline std::map<std::string, std::string> RegisteredAliases = {};

		[[nodiscard]] static const ConsoleCommand* Find(const std::string& a_Name);
		static void Dispatch(const ConsoleCommand& a_Command, const ConsoleArgs& a_Args);
		static void PrintUnknown(const std::string& a_Name);

		static void CMD_Help(const ConsoleArgs& a_Args);
		static void CMD_Version(const ConsoleArgs& a_Args);
		static void CMD_Unlimited(const ConsoleArgs& a_Args);
		static void CMD_Scale(const ConsoleArgs& a_Args);
	};
}
