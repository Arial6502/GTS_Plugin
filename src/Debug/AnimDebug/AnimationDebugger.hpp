#pragma once

#include "Debug/Trace/GraphIntrospect.hpp"

namespace GTS {

	// Two independent debug windows, toggled like the profiler: they draw from DebugWindow::DebugDraw
	// so they survive the debug window closing, and the manager keeps the context at kDebugNoInput so
	// they never eat gameplay input.
	class AnimationDebugger : public EventListener, public CInitSingleton<AnimationDebugger> {

		public:
		void OnSKSEDataLoaded() override;
		void OnMainUpdate() override;
		void OnActorAnimationChange(RE::Actor* a_Actor, const std::string_view& a_Tag, const std::string_view& a_Payload) override;
		void OnActor3DUnload(RE::Actor* a_Actor) override;
		void OnPluginReset() override;

		[[nodiscard]] static bool VarsEnabled();
		[[nodiscard]] static bool EventsEnabled();
		[[nodiscard]] static bool AnyEnabled();
		static void SetVarsEnabled(bool a_Enabled);
		static void SetEventsEnabled(bool a_Enabled);

		// Called from DebugWindow::DebugDraw, once per frame.
		static void Draw();

		// Fed from AnimationManager::StartAnim, including the refusals.
		static void Trigger(RE::Actor* a_Actor, std::string_view a_Trigger, std::string_view a_Detail, bool a_Sent);

		private:
		enum class Scope : std::int32_t {
			kPlayer,
			kTarget,
			kAll,
		};

		enum class RowKind : std::uint8_t {
			kTrigger,
			kAnnotation,
		};

		struct EventRow {
			double Time = 0.0;
			RE::FormID Actor = 0;
			std::string ActorName;
			RowKind Kind = RowKind::kAnnotation;
			std::string Tag;
			std::string Detail;
			bool Handled = false;
		};

		static void DrawVarWindow();
		static void DrawEventWindow();
		static void Capture(RE::Actor* a_Actor, RowKind a_Kind, std::string_view a_Tag, std::string_view a_Detail, bool a_Handled);
		static void Rebuild(RE::Actor* a_Actor);
		static void WriteLog(const EventRow& a_Row);
		static bool OpenLog();
		static void CloseLog();

		[[nodiscard]] static RE::Actor* Resolve(Scope a_Scope);
		[[nodiscard]] static bool InScope(RE::Actor* a_Actor);
		[[nodiscard]] static bool PassesNameFilter(std::string_view a_Name, const char* a_Filter);

		static constexpr std::size_t MaxRows = 4096;

		// Read on the game thread from the capture path, written on the render thread from the UI.
		static inline std::atomic<bool> m_ShowVars = false;
		static inline std::atomic<bool> m_ShowEvents = false;
		static inline std::atomic<bool> m_GtsOnly = false;
		static inline std::atomic<bool> m_Paused = false;
		static inline std::atomic<bool> m_LogToFile = false;
		static inline std::atomic<bool> m_WantTriggers = true;
		static inline std::atomic<bool> m_WantAnnotations = true;
		static inline std::atomic<Scope> m_EventScope = Scope::kPlayer;

		static inline std::atomic<Scope> m_VarScope = Scope::kPlayer;
		static inline std::atomic<bool> m_Rebuild = false;
		static inline std::atomic<std::size_t> m_Dropped = 0;

		// Handoff. The game thread publishes under the lock once a frame and the render thread takes
		// a copy, so neither side holds it across a poll or a draw.
		static inline std::mutex m_Lock;
		static inline std::vector<EventRow> m_Incoming = {};
		static inline std::vector<GraphVarSlot> m_Published = {};

		// Game thread only. Never touched from Draw.
		static inline std::vector<GraphVarSlot> m_Polled = {};
		static inline RE::FormID m_Built = 0;

		// Render thread only.
		static inline std::deque<EventRow> m_Rows = {};
		static inline std::vector<GraphVarSlot> m_View = {};
		static inline std::shared_ptr<spdlog::logger> m_Log = nullptr;
	};
}
