#pragma once

namespace GTS {

	//Per actor filter.
	enum class DrawTarget : uint8_t {
		kPlayerOnly,
		kPlayerAndFollowers,
		kAnyGTS,
		kAll,
	};

	namespace DebugCol {
		inline constexpr ImU32 White   = IM_COL32(255, 255, 255, 255);
		inline constexpr ImU32 Black   = IM_COL32(0, 0, 0, 255);
		inline constexpr ImU32 Grey    = IM_COL32(150, 150, 150, 255);
		inline constexpr ImU32 Red     = IM_COL32(235, 60, 60, 255);
		inline constexpr ImU32 Green   = IM_COL32(60, 220, 90, 255);
		inline constexpr ImU32 Blue    = IM_COL32(70, 130, 245, 255);
		inline constexpr ImU32 Yellow  = IM_COL32(240, 220, 60, 255);
		inline constexpr ImU32 Cyan    = IM_COL32(60, 220, 230, 255);
		inline constexpr ImU32 Magenta = IM_COL32(230, 80, 220, 255);
		inline constexpr ImU32 Orange  = IM_COL32(245, 150, 50, 255);
		inline constexpr ImU32 Lime    = IM_COL32(170, 240, 60, 255);
		inline constexpr ImU32 Purple  = IM_COL32(160, 100, 240, 255);
	}

	//Bundles the trailing arguments every primitive would otherwise repeat.
	//  DebugDraw::Sphere(pos, 48.0f, { .Color = DebugCol::Red, .LifetimeMs = 2000 });
	struct DebugStyle {
		ImU32 Color = DebugCol::Lime;
		float Thickness = 1.5f;

		//0 means this frame only.
		std::uint32_t LifetimeMs = 0;
	};

	struct DebugStats {
		std::uint32_t Submitted = 0;       //Primitives accepted this frame.
		std::uint32_t Dropped = 0;         //Rejected for exceeding iMaxPrimitives.
		std::uint32_t Live = 0;            //Primitives in the buffer, timed leftovers included.
		std::uint32_t Timed = 0;           //Of those, the ones on a lifetime.
		std::uint32_t Tracked = 0;         //Of those, the ones held by a tracked group.
		std::uint32_t Batches = 0;         //AddPolyline calls issued.
		std::uint32_t Segments = 0;        //Screen space segments after clipping.
		std::uint32_t Projections = 0;
		std::uint32_t CulledDistance = 0;
		std::uint32_t CulledBehind = 0;
		std::uint32_t CulledOffscreen = 0;
		std::uint32_t PointsUsed = 0;

		float RenderMs = 0.0f;

		static constexpr std::size_t kHistory = 120;
		std::array<float, kHistory> RenderMsHistory{};
		std::uint32_t HistoryPos = 0;

		void PushSample(float a_ms) {
			RenderMs = a_ms;
			RenderMsHistory[HistoryPos] = a_ms;
			HistoryPos = (HistoryPos + 1) % kHistory;
		}

		void ResetFrame() {
			Submitted = 0;
			Dropped = 0;
			Batches = 0;
			Segments = 0;
			Projections = 0;
			CulledDistance = 0;
			CulledBehind = 0;
			CulledOffscreen = 0;
		}
	};
}
