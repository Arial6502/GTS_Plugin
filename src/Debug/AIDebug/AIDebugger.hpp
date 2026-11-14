#pragma once

#include "Managers/AI/Brain.hpp"

namespace GTS {

	// Two debug windows for the action AI, built the same way as AnimationDebugger: the game thread
	// publishes a snapshot once a frame, the render thread draws from its own copy, and both windows
	// draw from DebugWindow::DebugDraw so they outlive the debug window closing.
	class AIDebugger : public EventListener, public CInitSingleton<AIDebugger> {

		public:
		void OnMainUpdate() override;
		void OnPluginReset() override;

		[[nodiscard]] static bool BrainsEnabled();
		[[nodiscard]] static bool EventsEnabled();
		[[nodiscard]] static bool AnyEnabled();
		static void SetBrainsEnabled(bool a_Enabled);
		static void SetEventsEnabled(bool a_Enabled);

		// Called from DebugWindow::DebugDraw, once per frame.
		static void Draw();

		// Every line the Director logs, whether or not action watch is on.
		static void Record(RE::FormID a_Actor, std::string_view a_Name, std::string_view a_Line);

		private:
		struct EventRow {
			double Time = 0.0;
			RE::FormID Actor = 0;
			std::string Name;
			std::string Kind;
			std::string Detail;
		};

		static void DrawBrainWindow();
		static void DrawEventWindow();
		static void DrawBrain(const AI::BrainView& a_View);

		static constexpr std::size_t MaxRows = 2048;

		static inline std::atomic<bool> m_ShowBrains = false;
		static inline std::atomic<bool> m_ShowEvents = false;
		static inline std::atomic<bool> m_Paused = false;

		// Handoff. Written on the game thread under the lock, copied out on the render thread.
		static inline std::mutex m_Lock;
		static inline std::vector<AI::BrainView> m_Published = {};
		static inline std::vector<EventRow> m_Incoming = {};

		// Render thread only.
		static inline std::vector<AI::BrainView> m_View = {};
		static inline std::deque<EventRow> m_Rows = {};
	};
}
