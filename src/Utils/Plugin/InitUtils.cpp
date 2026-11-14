#include "Utils/Plugin/InitUtils.hpp"
#include "Version.hpp"
#include "git.h"

namespace GTS {

	void InitUtils::OnSKSEDataLoaded() {
		CPrintPluginInfo();
	}

	void InitUtils::OnSKSEPostLoad() {
		LogPrintPluginInfo();
	}

	void InitUtils::VersionCheck() {
		const auto version = REL::Module::get().version();
		const auto isCompatible = version == SKSE::RUNTIME_SSE_1_5_97 || version == SKSE::RUNTIME_SSE_1_6_1170 || version == SKSE::RUNTIME_SSE_1_7_104;

		if (!isCompatible) {
			ReportAndExit(
				"Incompatible Game Version. Only versions:\n"
				"1.5.97, 1.6.1170 and 1.7.104\n"
	            "Are supported."
			);
		}
	}

	void InitUtils::CPrintPluginInfo() {
		Cprint("[GTSPlugin.dll]: [ Giantess Mod {} was succesfully initialized. Waiting for New Game/Save Load. ]", GTSPlugin::ModVersion.string());
		Cprint("[GTSPlugin.dll]: Dll Build Date: {} {}", __DATE__, __TIME__);
		Cprint("[GTSPlugin.dll]: Git Info:");
		Cprint("\t -- Commit: {}", git_CommitSubject());
		Cprint("\t -- SHA1: {}", git_CommitSHA1());
		Cprint("\t -- Date: {}", git_CommitDate());
		Cprint("\t -- Uncommited Changes: {}", git_AnyUncommittedChanges() ? "Yes" : "No");
	}

	void InitUtils::LogPrintPluginInfo() {

		logger::info("GTSPlugin {}", GTSPlugin::ModVersion.string());
		logger::info("Dll Build Date: {} {}", __DATE__, __TIME__);

		const std::string git_commit = fmt::format("\t -- Commit: {}", git_CommitSubject());
		const std::string git_sha1 = fmt::format("\t -- SHA1: {}", git_CommitSHA1());
		const std::string git_date = fmt::format("\t -- Date: {}", git_CommitDate());
		const std::string git_ditry = fmt::format("\t -- Uncommited Changes: {}", git_AnyUncommittedChanges() ? "Yes" : "No");

		logger::info("Git Info:\n{}\n{}\n{}\n{}", git_commit, git_sha1, git_date, git_ditry);
	}

}