#pragma once

namespace GTS {

	class AnimationTracer : public EventListener, public CInitSingleton<AnimationTracer> {

		public:
		void OnSKSEDataLoaded() override;
		void OnMainUpdate() override;
		void OnPluginReset() override;
		void OnActor3DUnload(RE::Actor* a_Actor) override;

		[[nodiscard]] static bool Capturing();

		static void Trigger(RE::Actor* a_Actor, std::string_view a_Trigger, std::string_view a_Behaviour, bool a_Accepted);
		static void Annotation(RE::Actor* a_Actor, std::string_view a_Tag, bool a_Handled);

		static bool Start(RE::Actor* a_Actor);
		static void Stop();
		static bool DumpInventory(RE::Actor* a_Actor);

		private:
		enum class VarKind : std::uint8_t {
			kNone,
			kBool,
			kInt,
			kFloat,
		};

		struct VarSlot {
			RE::BSFixedString Name;
			// BSFixedString interning is case-insensitive and hands back whatever spelling was
			// interned first, so the graph's own name is kept for the log.
			std::string Display;
			VarKind Kind = VarKind::kNone;
			std::uint32_t Raw = 0;
		};

		static bool BuildSlots(RE::Actor* a_Actor);
		static void PollVariables(RE::Actor* a_Actor);
		static void OpenLog();
		static void Write(std::string_view a_Line);

		[[nodiscard]] static std::string Describe(const VarSlot& a_Slot, std::uint32_t a_Raw);

		static inline std::shared_ptr<spdlog::logger> m_Log = nullptr;
		// The 3d unload that stops a capture arrives on the thread doing the unloading, while the
		// capture itself is written from the main thread. Recursive because these call each other:
		// Stop writes a line, Start builds slots and writes one.
		static inline std::recursive_mutex m_Lock;

		static inline std::vector<VarSlot> m_Slots = {};
		static inline RE::FormID m_Target = 0;
		static inline std::uint64_t m_Frame = 0;
	};
}
