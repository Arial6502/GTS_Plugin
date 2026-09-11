#include "Actions/Core/ActionLog.hpp"

namespace {
	constexpr std::string_view LogName = "GTSPluginActions";
}

namespace GTS::Actions {

	bool ActionLog::Open() {

		if (m_Log) {
			return true;
		}

		auto path = logger::log_directory();
		if (!path) {
			logger::error("ActionLog: no log directory");
			return false;
		}

		*path /= LogName;
		*path += L".log";

		try {
			m_Log = std::make_shared<spdlog::logger>("Actions", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
			m_Log->set_pattern("%v");
			m_Log->set_level(spdlog::level::trace);
			m_Log->flush_on(spdlog::level::trace);
		}
		catch (const std::exception& e) {
			logger::error("ActionLog: could not open {}.log: {}", LogName, e.what());
			m_Log = nullptr;
			return false;
		}

		return true;
	}

	void ActionLog::Close() {
		m_Log = nullptr;
	}

	bool ActionLog::IsOpen() {
		return m_Log != nullptr;
	}

	void ActionLog::SetFrame(std::uint64_t a_Frame) {
		m_Frame = a_Frame;
	}

	void ActionLog::Write(std::string_view a_Line) {
		if (m_Log) {
			m_Log->trace("{:>8}|{:.3f}|{}", m_Frame, Time::WorldTimeElapsed(), a_Line);
		}
	}
}
