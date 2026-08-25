#pragma once

#include "Debug/DebugTypes.hpp"
#include "Debug/DebugProjector.hpp"

namespace GTS {

	//World space debug geometry rendered through ImGui
	class DebugDraw final : public CInitSingleton<DebugDraw> {

		public:

		//Freeze keeps rendering what is already buffered but stops accepting new work.
		[[nodiscard]] static bool IsFrozen() noexcept {
			return GetSingleton().m_frozen.load(std::memory_order_relaxed);
		}

		static void SetFrozen(bool a_frozen) noexcept {
			GetSingleton().m_frozen.store(a_frozen, std::memory_order_relaxed);
		}

		[[nodiscard]] static const DebugStats& GetStats() noexcept {
			return GetSingleton().m_stats;
		}

		[[nodiscard]] static const DebugProjector& GetProjector() noexcept {
			return GetSingleton().m_projector;
		}

		static void Clear();

		// ---------------------------------------------------------------------
		// Gates
		// ---------------------------------------------------------------------

		[[nodiscard]] static bool Active() noexcept; //Is the debug overlay enabled?
		[[nodiscard]] static bool Wants() noexcept;  //Replacement for CanDraw(), Use this to gate debug draw checks.
		[[nodiscard]] static bool Wants(RE::Actor* a_actor, DrawTarget a_target);
		[[nodiscard]] static bool Matches(RE::Actor* a_actor, DrawTarget a_target);

		// ---------------------------------------------------------------------
		// Tracked groups
		// ---------------------------------------------------------------------
		//
		//  {
		//      DebugDraw::TrackedScope scope{ DebugDraw::TrackKey("CharController", actor->formID) };
		//      DebugDraw::Capsule(a, b, r, { .Color = DebugCol::Cyan });
		//  }
		//
		// Tracked groups can be used to key specific debug draw calls so they don't suffer temporal issues like flickering or ghosting.
		// Tracking is RAII so to effectively use you need to wrap things in braces, a bit like the ImUtil_Unique Macro does.
		[[nodiscard]] static constexpr std::uint64_t TrackKey(std::string_view a_tag, std::uint32_t a_id = 0) {

			std::uint64_t hash = 14695981039346656037ull;

			for (const char c : a_tag) {
				hash ^= static_cast<std::uint8_t>(c);
				hash *= 1099511628211ull;
			}

			hash ^= static_cast<std::uint64_t>(a_id) + 0x9E3779B97F4A7C15ull + (hash << 6) + (hash >> 2);
			return hash ? hash : 1ull;
		}

		class TrackedScope {

			public:
			explicit TrackedScope(std::uint64_t a_key) noexcept;
			~TrackedScope() noexcept;

			TrackedScope(const TrackedScope&) = delete;
			TrackedScope& operator=(const TrackedScope&) = delete;

			private:
			std::uint64_t m_previous = 0;
		};

		//Drops a tracked group at the next render without waiting for its timeout.
		static void Untrack(std::uint64_t a_key);

		// ----------------------------
		// Primitives (Public Draw API)
		// ----------------------------
		
		//All coordinates are in world-space.

		static void Line(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, const DebugStyle& a_style = {});

		//a_closed rings the last vertex back to the first.
		static void Polyline(std::span<const RE::NiPoint3> a_points, bool a_closed = false, const DebugStyle& a_style = {});

		static void Triangle(const RE::NiPoint3& a_a, const RE::NiPoint3& a_b, const RE::NiPoint3& a_c, const DebugStyle& a_style = {});
		static void Quad(const RE::NiPoint3& a_a, const RE::NiPoint3& a_b, const RE::NiPoint3& a_c, const RE::NiPoint3& a_d, const DebugStyle& a_style = {});

		//Circle on the plane whose normal is a_normal. a_normal need not be unit length.
		static void Circle(const RE::NiPoint3& a_center, float a_radius, const RE::NiPoint3& a_normal, const DebugStyle& a_style = {}, int a_segments = 32);

		//Three orthogonal rings. Reads well at any angle.
		static void Sphere(const RE::NiPoint3& a_center, float a_radius, const DebugStyle& a_style = {}, int a_segments = 32);

		//Cylinder plus two hemispherical caps, the shape Havok character controllers actually use.
		static void Capsule(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float a_radius, const DebugStyle& a_style = {}, int a_segments = 16, int a_rings = 3);

		static void Box(const RE::NiPoint3& a_center, const RE::NiPoint3& a_halfExtents, const RE::NiMatrix3& a_rotation, const DebugStyle& a_style = {});
		static void Box(const RE::NiTransform& a_transform, const RE::NiPoint3& a_halfExtents, const DebugStyle& a_style = {});

		//Axis aligned, given opposite corners in either order.
		static void AABB(const RE::NiPoint3& a_min, const RE::NiPoint3& a_max, const DebugStyle& a_style = {});

		static void Bound(const RE::NiBound& a_bound, const DebugStyle& a_style = {}, int a_segments = 32);

		//Flat rectangle on the XY plane, yaw in radians.
		static void Rect(const RE::NiPoint3& a_center, float a_yaw, float a_width, float a_length, const DebugStyle& a_style = {});
		static void Rhomb(const RE::NiPoint3& a_center, float a_radius, float a_yaw, const DebugStyle& a_style = {});

		//Line plus a four sided head sized as a fraction of the shaft length.
		static void Arrow(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, const DebugStyle& a_style = {}, float a_headScale = 0.15f);

		//Three axis aligned arms through a_pos.
		static void Cross(const RE::NiPoint3& a_pos, float a_size, const DebugStyle& a_style = {});

		//Local X/Y/Z of a transform in red/green/blue.
		static void Axes(const RE::NiTransform& a_transform, float a_length, float a_thickness = 1.5f, std::uint32_t a_lifetimeMs = 0);

		//Screen space dot at a world position. Radius is in pixels and does not scale with range.
		static void Marker(const RE::NiPoint3& a_pos, float a_radiusPx = 3.0f, const DebugStyle& a_style = {}, bool a_filled = true);

		//Truncated at 255 bytes. Anything longer belongs in the log.
		static void Text(const RE::NiPoint3& a_pos, std::string_view a_text, const DebugStyle& a_style = {}, bool a_centered = true);

		static void PointCloud(std::span<const RE::NiPoint3> a_points, float a_radiusPx = 2.0f, const DebugStyle& a_style = {});

		//Vertex markers plus the enclosing box. No topology assumed, the fallback for a point soup whose shape cannot be recovered.
		static void Hull(std::span<const RE::NiPoint3> a_vertices, const DebugStyle& a_style = {}, float a_vertexRadiusPx = 2.0f);

		//Wireframe for a convex point set that is layered along Z, which is what Havok character
		//controller shapes are. Vertices are grouped into height bands, each band is ordered
		//around the shared centroid and rung as a loop, consecutive bands are joined, and a band
		//of one or two vertices is treated as an apex and spoked to its neighbour.
		//Falls back to Hull when fewer than two bands come out.
		static void ConvexShape(std::span<const RE::NiPoint3> a_vertices, const DebugStyle& a_style = {});

		// ---------------------------------------------------------------------
		// Node helpers
		// ---------------------------------------------------------------------

		//Marker at the node's world position, optionally with its local axes.
		static void Node(RE::NiAVObject* a_node, const DebugStyle& a_style = {}, float a_axisLength = 0.0f);

		struct SkeletonOptions {
			//A modded skeleton buries real bones under extra CME and XPMSE nodes, so a shallow
			//limit truncates the arm and leg chains rather than the noise.
			int MaxDepth = 64;

			//Skinned BSGeometry carries an identity local transform, so its world position is
			//exactly its parent's and drawing to it puts a zero length line on the joint.
			bool NodesOnly = true;

			bool SkipDegenerate = true;
			float MinBoneLength = 0.5f;

			bool DrawJoints = true;
			bool DrawNames = false;
		};

		//Parent to child lines for the whole subtree.
		static void Skeleton(RE::NiAVObject* a_root, const DebugStyle& a_style = {}, const SkeletonOptions& a_options = {});

		// ---------------------------------------------------------------------
		// Render hooks
		// ---------------------------------------------------------------------

		//Invoked at the top of Render(), before the frame's submissions are drained, so anything a
		//hook submits lands in the same frame rather than the next one. Useful for geometry that
		//should be sampled as late as possible instead of during the caller's own update.
		//
		//Runs on the same caller as Render(), so a hook needs no locking of its own.
		//
		//Skipped while the overlay is disabled or frozen.
		using RenderHook = std::function<void()>;

		static std::uint32_t AddRenderHook(RenderHook a_hook);
		static void RemoveRenderHook(std::uint32_t a_id);

		//Driven by DebugOverlayWindow::Draw.
		void Render();

		private:
		static constexpr std::size_t kMaxTextLen = 255;

		//Segments crossing the viewport edge are clipped against a rect inflated by this much, so
		//joins just off screen still bend the way they should.
		static constexpr float kClipMargin = 64.0f;

		enum class Kind : std::uint8_t {
			kPolyline,
			kMarker,
			kText
		};

		struct Command {
			std::uint32_t First = 0;     //First vertex in Buffer::Points.
			std::uint32_t Count = 0;     //Markers and text use one.
			std::uint32_t StrFirst = 0;  //First byte in Buffer::Strings, text only.
			std::uint16_t StrLen = 0;
			ImU32 Color = 0;
			float Thickness = 1.0f;
			float Radius = 0.0f;         //Marker radius in pixels.
			std::uint64_t Expiry = 0;    //Absolute ms. 0 means this frame only.
			std::uint64_t Key = 0;       //Tracked group. 0 means untracked.
			Kind Type = Kind::kPolyline;
			bool Closed = false;
			bool Filled = false;
			bool Centered = false;
		};

		//Flat arenas rather than a vector per primitive. One growth curve, no per submission
		//allocation once warm, and retiring a frame's worth is a single linear pass.
		struct Buffer {
			std::vector<Command> Commands;
			std::vector<RE::NiPoint3> Points;
			std::vector<char> Strings;

			void Clear() {
				Commands.clear();
				Points.clear();
				Strings.clear();
			}
		};

		//Whether the buffers will take a submission right now. Broader than Wants(): a feature
		//drawing under its own toggle must get through even with the master switch off.
		[[nodiscard]] static bool Accepting() noexcept;

		[[nodiscard]] static std::uint64_t NowMs();

		//Every primitive funnels through these three.
		static void Push(std::span<const RE::NiPoint3> a_points, bool a_closed, const DebugStyle& a_style);
		static void PushMarker(const RE::NiPoint3& a_pos, float a_radiusPx, const DebugStyle& a_style, bool a_filled);
		static void PushText(const RE::NiPoint3& a_pos, std::string_view a_text, const DebugStyle& a_style, bool a_centered);

		//Returns the command slot to fill, or null when the frame budget is spent.
		Command* Reserve(const DebugStyle& a_style, std::uint32_t a_pointCount);

		//Appends a_src onto a_dst, rebasing arena indices.
		static void Merge(Buffer& a_dst, Buffer& a_src);

		//Rebuilds m_active around the commands a_keep accepts, compacting the arenas as it goes.
		template <typename Pred>
		void Compact(Pred a_keep);

		//Drops frame scoped and expired commands.
		void Retire(std::uint64_t a_nowMs);

		//Evicts the previous batch of every tracked group present in m_pending, plus anything
		//Untrack() has queued. Runs before the merge, so a group is replaced rather than stacked.
		void EvictReplacedGroups();

		void RenderPolyline(ImDrawList* a_list, const Command& a_cmd, ImU32 a_col, float a_thickness);
		void RenderMarker(ImDrawList* a_list, const Command& a_cmd, ImU32 a_col);
		void RenderText(ImDrawList* a_list, const Command& a_cmd, ImU32 a_col);
		void RenderStatsOverlay(ImDrawList* a_list) const;

		void InvokeRenderHooks();
		void FlushRun(ImDrawList* a_list, ImU32 a_col, float a_thickness);

		//Liang-Barsky against m_clipMin/m_clipMax. False when the segment misses entirely.
		[[nodiscard]] bool ClipSegment(ImVec2& a_from, ImVec2& a_to) const;

		DebugStats m_stats{};
		DebugProjector m_projector{};

		std::atomic<bool> m_frozen{ false };

		//Written from any thread under m_lock, drained once per frame.
		std::mutex m_lock;
		Buffer m_staging;

		//Published by Render() so submitters can budget without touching m_active.
		std::atomic<std::uint32_t> m_activeCount{ 0 };

		//Keys queued by Untrack, applied at the next render.
		std::mutex m_untrackLock;
		std::vector<std::uint64_t> m_untrackQueue;

		std::mutex m_hookLock;
		std::vector<std::pair<std::uint32_t, RenderHook>> m_renderHooks;
		std::uint32_t m_nextHookId = 0;

		//Touched only by Render().
		Buffer m_active;
		Buffer m_pending;
		Buffer m_scratch;

		//Scratch reused every frame so the render path stays allocation free once warm.
		std::vector<ImVec2> m_projected;
		std::vector<std::uint8_t> m_valid;
		std::vector<ImVec2> m_run;

		//Keys carried by the incoming batch, rebuilt each frame.
		absl::flat_hash_set<std::uint64_t> m_replacedKeys;

		ImVec2 m_clipMin{ 0.0f, 0.0f };
		ImVec2 m_clipMax{ 0.0f, 0.0f };
	};
}
