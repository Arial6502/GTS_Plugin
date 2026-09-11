#pragma once

namespace GTS::Actions {

	class ActionLog {

		public:
		static bool Open();
		static void Close();
		[[nodiscard]] static bool IsOpen();

		static void SetFrame(std::uint64_t a_Frame);
		static void Write(std::string_view a_Line);

		private:
		static inline std::shared_ptr<spdlog::logger> m_Log = nullptr;
		static inline std::uint64_t m_Frame = 0;
	};
}
