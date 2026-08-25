#include "Debug/DebugDraw.hpp"

#include "Config/Config.hpp"
#include "UI/Core/ImColorUtils.hpp"

namespace {

	constexpr float kPi = std::numbers::pi_v<float>;
	constexpr float kTwoPi = kPi * 2.0f;
	constexpr float kHalfPi = kPi * 0.5f;

	//Current tracked group, per thread so two threads submitting different groups cannot interleave their keys.
	thread_local std::uint64_t t_trackKey = 0;

	//ConvexShape band detection.
	constexpr float kWeldEpsilon = 0.001f;
	constexpr float kMinBandTolerance = 0.5f;
	constexpr float kBandFraction = 0.02f;
	constexpr float kSpokeThicknessScale = 0.7f;

	//Two axes perpendicular to a_normal.
	void BuildBasis(const RE::NiPoint3& a_normal, RE::NiPoint3& a_u, RE::NiPoint3& a_v) {

		RE::NiPoint3 n = a_normal;
		const float len = n.Length();

		if (len < 1e-6f) {
			n = RE::NiPoint3(0.0f, 0.0f, 1.0f);
		}
		else {
			n /= len;
		}

		if (n.z < -0.9999999f) {
			a_u = RE::NiPoint3(0.0f, -1.0f, 0.0f);
			a_v = RE::NiPoint3(-1.0f, 0.0f, 0.0f);
			return;
		}

		const float a = 1.0f / (1.0f + n.z);
		const float b = -n.x * n.y * a;

		a_u = RE::NiPoint3(1.0f - n.x * n.x * a, b, -n.x);
		a_v = RE::NiPoint3(b, 1.0f - n.y * n.y * a, -n.y);
	}

	bool NearlyEqual(const ImVec2& a_lhs, const ImVec2& a_rhs) {
		constexpr float eps = 0.05f;
		return std::fabs(a_lhs.x - a_rhs.x) < eps && std::fabs(a_lhs.y - a_rhs.y) < eps;
	}

	void WalkSkeleton(RE::NiAVObject* a_node, const GTS::DebugStyle& a_style, const GTS::DebugDraw::SkeletonOptions& a_options, int a_depth) {

		RE::NiNode* asNode = a_node->AsNode();

		if (!asNode || a_depth >= a_options.MaxDepth) {
			return;
		}

		const float minLengthSq = a_options.MinBoneLength * a_options.MinBoneLength;

		for (const auto& child : asNode->GetChildren()) {

			RE::NiAVObject* ptr = child.get();

			if (!ptr) {
				continue;
			}

			if (a_options.NodesOnly && !ptr->AsNode()) {
				continue;
			}

			const RE::NiPoint3& from = a_node->world.translate;
			const RE::NiPoint3& to = ptr->world.translate;

			if (!a_options.SkipDegenerate || from.GetSquaredDistance(to) >= minLengthSq) {
				GTS::DebugDraw::Line(from, to, a_style);
			}

			if (a_options.DrawJoints) {
				GTS::DebugDraw::Marker(to, 2.0f, a_style, true);
			}

			if (a_options.DrawNames && !ptr->name.empty()) {
				GTS::DebugDraw::Text(to, ptr->name.c_str(), a_style, true);
			}

			WalkSkeleton(ptr, a_style, a_options, a_depth + 1);
		}
	}
}

namespace GTS {

	std::uint64_t DebugDraw::NowMs() {
		const auto since = std::chrono::steady_clock::now().time_since_epoch();
		return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(since).count());
	}

	void DebugDraw::Clear() {

		auto& self = GetSingleton();
		const std::lock_guard lock{ self.m_lock };

		self.m_staging.Clear();
		self.m_pending.Clear();
		self.m_active.Clear();
		self.m_activeCount.store(0, std::memory_order_relaxed);
		self.m_stats.Live = 0;
		self.m_stats.Timed = 0;
		self.m_stats.Tracked = 0;
	}

	// -------------------------------------------------------------------------
	// Tracked groups
	// -------------------------------------------------------------------------

	DebugDraw::TrackedScope::TrackedScope(std::uint64_t a_key) noexcept : m_previous(t_trackKey) {
		t_trackKey = a_key;
	}

	DebugDraw::TrackedScope::~TrackedScope() noexcept {
		t_trackKey = m_previous;
	}

	void DebugDraw::Untrack(std::uint64_t a_key) {

		if (a_key == 0ull) {
			return;
		}

		auto& self = GetSingleton();
		const std::lock_guard lock{ self.m_untrackLock };

		self.m_untrackQueue.push_back(a_key);
	}

	void DebugDraw::EvictReplacedGroups() {

		m_replacedKeys.clear();

		for (const Command& cmd : m_pending.Commands) {
			if (cmd.Key != 0ull) {
				m_replacedKeys.insert(cmd.Key);
			}
		}

		{
			const std::lock_guard lock{ m_untrackLock };

			for (const std::uint64_t key : m_untrackQueue) {
				m_replacedKeys.insert(key);
			}

			m_untrackQueue.clear();
		}

		if (m_replacedKeys.empty()) {
			return;
		}

		Compact([this](const Command& a_cmd) {
			return a_cmd.Key == 0ull || !m_replacedKeys.contains(a_cmd.Key);
		});
	}

	// -------------------------------------------------------------------------
	// Gates
	// -------------------------------------------------------------------------

	bool DebugDraw::Active() noexcept {

		return Config::Advanced.bShowOverlay
			|| Config::Collision.bDrawDebugShapes
			|| Config::AutoAim.bDebugAutoAim;
	}

	bool DebugDraw::Accepting() noexcept {
		return Active() && !IsFrozen();
	}

	bool DebugDraw::Wants() noexcept {
		return Config::Advanced.bShowOverlay && !IsFrozen();
	}

	bool DebugDraw::Matches(RE::Actor* a_actor, DrawTarget a_target) {

		if (!a_actor) {
			return false;
		}

		switch (a_target) {

			default:
			case DrawTarget::kPlayerOnly: {
				return a_actor->IsPlayerRef();
			}

			case DrawTarget::kPlayerAndFollowers: {
				return a_actor->IsPlayerRef() || IsTeammate(a_actor);
			}

			case DrawTarget::kAnyGTS: {
				return a_actor->IsPlayerRef() || IsTeammate(a_actor) || EffectsForEveryone(a_actor);
			}

			case DrawTarget::kAll: {
				return true;
			}
		}
	}

	bool DebugDraw::Wants(RE::Actor* a_actor, DrawTarget a_target) {
		return Wants() && Matches(a_actor, a_target);
	}

	// -------------------------------------------------------------------------
	// Submission
	// -------------------------------------------------------------------------

	DebugDraw::Command* DebugDraw::Reserve(const DebugStyle& a_style, std::uint32_t a_pointCount) {

		//Caller holds m_lock.
		const std::size_t live = m_staging.Commands.size() + m_activeCount.load(std::memory_order_relaxed);

		if (live >= static_cast<std::size_t>(std::max(Config::Advanced.Overlay.iMaxPrimitives, 1))) {
			++m_stats.Dropped;
			return nullptr;
		}

		Command& cmd = m_staging.Commands.emplace_back();

		cmd.First = static_cast<std::uint32_t>(m_staging.Points.size());
		cmd.Count = a_pointCount;
		cmd.Color = a_style.Color;
		cmd.Thickness = a_style.Thickness;
		cmd.Key = t_trackKey;

		//A tracked command outlives the gap between submissions instead of the caller's
		//lifetime, so a frame drop cannot expire it before the next batch replaces it.
		if (cmd.Key != 0ull) {
			const auto timeout = static_cast<std::uint64_t>(std::max(Config::Advanced.Overlay.iTrackedTimeoutMs, 1));
			cmd.Expiry = NowMs() + timeout;
		}
		else {
			cmd.Expiry = a_style.LifetimeMs ? NowMs() + a_style.LifetimeMs : 0ull;
		}

		++m_stats.Submitted;
		return &cmd;
	}

	void DebugDraw::Push(std::span<const RE::NiPoint3> a_points, bool a_closed, const DebugStyle& a_style) {

		if (a_points.size() < 2 || !Accepting()) {
			return;
		}

		auto& self = GetSingleton();
		const std::lock_guard lock{ self.m_lock };

		Command* cmd = self.Reserve(a_style, static_cast<std::uint32_t>(a_points.size()));

		if (!cmd) {
			return;
		}

		cmd->Type = Kind::kPolyline;
		cmd->Closed = a_closed;

		self.m_staging.Points.insert(self.m_staging.Points.end(), a_points.begin(), a_points.end());
	}

	void DebugDraw::PushMarker(const RE::NiPoint3& a_pos, float a_radiusPx, const DebugStyle& a_style, bool a_filled) {

		if (!Accepting()) {
			return;
		}

		auto& self = GetSingleton();
		const std::lock_guard lock{ self.m_lock };

		Command* cmd = self.Reserve(a_style, 1u);

		if (!cmd) {
			return;
		}

		cmd->Type = Kind::kMarker;
		cmd->Radius = a_radiusPx;
		cmd->Filled = a_filled;

		self.m_staging.Points.push_back(a_pos);
	}

	void DebugDraw::PushText(const RE::NiPoint3& a_pos, std::string_view a_text, const DebugStyle& a_style, bool a_centered) {

		if (a_text.empty() || !Accepting()) {
			return;
		}

		const std::size_t length = std::min(a_text.size(), kMaxTextLen);

		auto& self = GetSingleton();
		const std::lock_guard lock{ self.m_lock };

		Command* cmd = self.Reserve(a_style, 1u);

		if (!cmd) {
			return;
		}

		cmd->Type = Kind::kText;
		cmd->Centered = a_centered;
		cmd->StrFirst = static_cast<std::uint32_t>(self.m_staging.Strings.size());
		cmd->StrLen = static_cast<std::uint16_t>(length);

		self.m_staging.Points.push_back(a_pos);
		self.m_staging.Strings.insert(self.m_staging.Strings.end(), a_text.begin(), a_text.begin() + length);
	}

	// -------------------------------------------------------------------------
	// Primitives
	// -------------------------------------------------------------------------

	void DebugDraw::Line(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, const DebugStyle& a_style) {
		const std::array<RE::NiPoint3, 2> points{ a_from, a_to };
		Push(points, false, a_style);
	}

	void DebugDraw::Polyline(std::span<const RE::NiPoint3> a_points, bool a_closed, const DebugStyle& a_style) {
		Push(a_points, a_closed, a_style);
	}

	void DebugDraw::Triangle(const RE::NiPoint3& a_a, const RE::NiPoint3& a_b, const RE::NiPoint3& a_c, const DebugStyle& a_style) {
		const std::array<RE::NiPoint3, 3> points{ a_a, a_b, a_c };
		Push(points, true, a_style);
	}

	void DebugDraw::Quad(const RE::NiPoint3& a_a, const RE::NiPoint3& a_b, const RE::NiPoint3& a_c, const RE::NiPoint3& a_d, const DebugStyle& a_style) {
		const std::array<RE::NiPoint3, 4> points{ a_a, a_b, a_c, a_d };
		Push(points, true, a_style);
	}

	void DebugDraw::Circle(const RE::NiPoint3& a_center, float a_radius, const RE::NiPoint3& a_normal, const DebugStyle& a_style, int a_segments) {

		if (a_radius <= 0.0f || !Accepting()) {
			return;
		}

		const int segments = std::clamp(a_segments, 3, 256);

		RE::NiPoint3 u;
		RE::NiPoint3 v;
		BuildBasis(a_normal, u, v);

		absl::InlinedVector<RE::NiPoint3, 64> ring;
		ring.reserve(static_cast<std::size_t>(segments));

		for (int i = 0; i < segments; ++i) {
			const float t = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
			ring.push_back(a_center + u * (std::cos(t) * a_radius) + v * (std::sin(t) * a_radius));
		}

		Push(ring, true, a_style);
	}

	void DebugDraw::Sphere(const RE::NiPoint3& a_center, float a_radius, const DebugStyle& a_style, int a_segments) {

		if (a_radius <= 0.0f || !Accepting()) {
			return;
		}

		Circle(a_center, a_radius, RE::NiPoint3(1.0f, 0.0f, 0.0f), a_style, a_segments);
		Circle(a_center, a_radius, RE::NiPoint3(0.0f, 1.0f, 0.0f), a_style, a_segments);
		Circle(a_center, a_radius, RE::NiPoint3(0.0f, 0.0f, 1.0f), a_style, a_segments);
	}

	void DebugDraw::Capsule(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float a_radius, const DebugStyle& a_style, int a_segments, int a_rings) {

		if (a_radius <= 0.0f || !Accepting()) {
			return;
		}

		RE::NiPoint3 axis = a_to - a_from;
		const float length = axis.Length();

		if (length < 1e-4f) {
			Sphere(a_from, a_radius, a_style, a_segments * 2);
			return;
		}

		axis /= length;

		RE::NiPoint3 u;
		RE::NiPoint3 v;
		BuildBasis(axis, u, v);

		const int segments = std::clamp(a_segments, 4, 128);
		const int rings = std::clamp(a_rings, 1, 32);
		const float ringStep = kHalfPi / static_cast<float>(rings + 1);

		Circle(a_from, a_radius, axis, a_style, segments);
		Circle(a_to, a_radius, axis, a_style, segments);

		for (int j = 1; j <= rings; ++j) {

			const float phi = ringStep * static_cast<float>(j);
			const float ringRadius = a_radius * std::cos(phi);
			const float offset = a_radius * std::sin(phi);

			Circle(a_to + axis * offset, ringRadius, axis, a_style, segments);
			Circle(a_from - axis * offset, ringRadius, axis, a_style, segments);
		}

		const int meridians = std::min(segments, 8);

		absl::InlinedVector<RE::NiPoint3, 64> line;

		for (int m = 0; m < meridians; ++m) {

			const float theta = kTwoPi * static_cast<float>(m) / static_cast<float>(meridians);
			const RE::NiPoint3 radial = u * std::cos(theta) + v * std::sin(theta);

			line.clear();

			for (int j = rings + 1; j >= 0; --j) {
				const float phi = ringStep * static_cast<float>(j);
				line.push_back(a_from - axis * (a_radius * std::sin(phi)) + radial * (a_radius * std::cos(phi)));
			}

			for (int j = 0; j <= rings + 1; ++j) {
				const float phi = ringStep * static_cast<float>(j);
				line.push_back(a_to + axis * (a_radius * std::sin(phi)) + radial * (a_radius * std::cos(phi)));
			}

			Push(line, false, a_style);
		}
	}

	void DebugDraw::Box(const RE::NiPoint3& a_center, const RE::NiPoint3& a_halfExtents, const RE::NiMatrix3& a_rotation, const DebugStyle& a_style) {

		if (!Accepting()) {
			return;
		}

		const RE::NiPoint3 ex = a_rotation * RE::NiPoint3(a_halfExtents.x, 0.0f, 0.0f);
		const RE::NiPoint3 ey = a_rotation * RE::NiPoint3(0.0f, a_halfExtents.y, 0.0f);
		const RE::NiPoint3 ez = a_rotation * RE::NiPoint3(0.0f, 0.0f, a_halfExtents.z);

		const std::array<RE::NiPoint3, 4> bottom{
			a_center - ex - ey - ez,
			a_center + ex - ey - ez,
			a_center + ex + ey - ez,
			a_center - ex + ey - ez
		};

		const std::array<RE::NiPoint3, 4> top{
			bottom[0] + ez * 2.0f,
			bottom[1] + ez * 2.0f,
			bottom[2] + ez * 2.0f,
			bottom[3] + ez * 2.0f
		};

		Push(bottom, true, a_style);
		Push(top, true, a_style);

		for (std::size_t i = 0; i < 4; ++i) {
			const std::array<RE::NiPoint3, 2> riser{ bottom[i], top[i] };
			Push(riser, false, a_style);
		}
	}

	void DebugDraw::Box(const RE::NiTransform& a_transform, const RE::NiPoint3& a_halfExtents, const DebugStyle& a_style) {
		Box(a_transform.translate, a_halfExtents * a_transform.scale, a_transform.rotate, a_style);
	}

	void DebugDraw::AABB(const RE::NiPoint3& a_min, const RE::NiPoint3& a_max, const DebugStyle& a_style) {

		const RE::NiPoint3 center(
			(a_min.x + a_max.x) * 0.5f,
			(a_min.y + a_max.y) * 0.5f,
			(a_min.z + a_max.z) * 0.5f);

		const RE::NiPoint3 half(
			std::fabs(a_max.x - a_min.x) * 0.5f,
			std::fabs(a_max.y - a_min.y) * 0.5f,
			std::fabs(a_max.z - a_min.z) * 0.5f);

		Box(center, half, RE::NiMatrix3(), a_style);
	}

	void DebugDraw::Bound(const RE::NiBound& a_bound, const DebugStyle& a_style, int a_segments) {
		Sphere(a_bound.center, a_bound.radius, a_style, a_segments);
	}

	void DebugDraw::Rect(const RE::NiPoint3& a_center, float a_yaw, float a_width, float a_length, const DebugStyle& a_style) {

		if (!Accepting()) {
			return;
		}

		const RE::NiPoint3 forward(std::sin(a_yaw), std::cos(a_yaw), 0.0f);
		const RE::NiPoint3 right(forward.y, -forward.x, 0.0f);

		const RE::NiPoint3 halfRight = right * (a_width * 0.5f);
		const RE::NiPoint3 halfForward = forward * (a_length * 0.5f);

		const std::array<RE::NiPoint3, 4> corners{
			a_center - halfRight - halfForward,
			a_center + halfRight - halfForward,
			a_center + halfRight + halfForward,
			a_center - halfRight + halfForward
		};

		Push(corners, true, a_style);
	}

	void DebugDraw::Rhomb(const RE::NiPoint3& a_center, float a_radius, float a_yaw, const DebugStyle& a_style) {

		if (!Accepting()) {
			return;
		}

		const RE::NiPoint3 forward(std::sin(a_yaw), std::cos(a_yaw), 0.0f);
		const RE::NiPoint3 right(forward.y, -forward.x, 0.0f);

		const std::array<RE::NiPoint3, 4> corners{
			a_center + forward * a_radius,
			a_center + right * a_radius,
			a_center - forward * a_radius,
			a_center - right * a_radius
		};

		Push(corners, true, a_style);
	}

	void DebugDraw::Arrow(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, const DebugStyle& a_style, float a_headScale) {

		if (!Accepting()) {
			return;
		}

		RE::NiPoint3 axis = a_to - a_from;
		const float length = axis.Length();

		if (length < 1e-4f) {
			return;
		}

		axis /= length;

		RE::NiPoint3 u;
		RE::NiPoint3 v;
		BuildBasis(axis, u, v);

		const float head = length * std::clamp(a_headScale, 0.01f, 0.9f);
		const RE::NiPoint3 base = a_to - axis * head;
		const float spread = head * 0.4f;

		Line(a_from, a_to, a_style);

		const std::array<RE::NiPoint3, 4> barbs{
			base + u * spread,
			base - u * spread,
			base + v * spread,
			base - v * spread
		};

		for (const RE::NiPoint3& barb : barbs) {
			Line(barb, a_to, a_style);
		}
	}

	void DebugDraw::Cross(const RE::NiPoint3& a_pos, float a_size, const DebugStyle& a_style) {

		if (!Accepting()) {
			return;
		}

		Line(a_pos - RE::NiPoint3(a_size, 0.0f, 0.0f), a_pos + RE::NiPoint3(a_size, 0.0f, 0.0f), a_style);
		Line(a_pos - RE::NiPoint3(0.0f, a_size, 0.0f), a_pos + RE::NiPoint3(0.0f, a_size, 0.0f), a_style);
		Line(a_pos - RE::NiPoint3(0.0f, 0.0f, a_size), a_pos + RE::NiPoint3(0.0f, 0.0f, a_size), a_style);
	}

	void DebugDraw::Axes(const RE::NiTransform& a_transform, float a_length, float a_thickness, std::uint32_t a_lifetimeMs) {

		if (a_length <= 0.0f || !Accepting()) {
			return;
		}

		const RE::NiPoint3& origin = a_transform.translate;

		const RE::NiPoint3 x = a_transform.rotate * RE::NiPoint3(a_length, 0.0f, 0.0f);
		const RE::NiPoint3 y = a_transform.rotate * RE::NiPoint3(0.0f, a_length, 0.0f);
		const RE::NiPoint3 z = a_transform.rotate * RE::NiPoint3(0.0f, 0.0f, a_length);

		Line(origin, origin + x, { .Color = DebugCol::Red,   .Thickness = a_thickness, .LifetimeMs = a_lifetimeMs });
		Line(origin, origin + y, { .Color = DebugCol::Green, .Thickness = a_thickness, .LifetimeMs = a_lifetimeMs });
		Line(origin, origin + z, { .Color = DebugCol::Blue,  .Thickness = a_thickness, .LifetimeMs = a_lifetimeMs });
	}

	void DebugDraw::Marker(const RE::NiPoint3& a_pos, float a_radiusPx, const DebugStyle& a_style, bool a_filled) {
		PushMarker(a_pos, a_radiusPx, a_style, a_filled);
	}

	void DebugDraw::Text(const RE::NiPoint3& a_pos, std::string_view a_text, const DebugStyle& a_style, bool a_centered) {
		PushText(a_pos, a_text, a_style, a_centered);
	}

	void DebugDraw::PointCloud(std::span<const RE::NiPoint3> a_points, float a_radiusPx, const DebugStyle& a_style) {

		if (!Accepting()) {
			return;
		}

		for (const RE::NiPoint3& point : a_points) {
			PushMarker(point, a_radiusPx, a_style, true);
		}
	}

	void DebugDraw::Hull(std::span<const RE::NiPoint3> a_vertices, const DebugStyle& a_style, float a_vertexRadiusPx) {

		if (a_vertices.empty() || !Accepting()) {
			return;
		}

		RE::NiPoint3 min = a_vertices.front();
		RE::NiPoint3 max = a_vertices.front();

		for (const RE::NiPoint3& v : a_vertices) {
			min.x = std::min(min.x, v.x);
			min.y = std::min(min.y, v.y);
			min.z = std::min(min.z, v.z);
			max.x = std::max(max.x, v.x);
			max.y = std::max(max.y, v.y);
			max.z = std::max(max.z, v.z);
		}

		DebugStyle boxStyle = a_style;
		boxStyle.Color = ImUtil::Colors::WithAlpha(a_style.Color, 0.45f);

		AABB(min, max, boxStyle);
		PointCloud(a_vertices, a_vertexRadiusPx, a_style);
	}

	void DebugDraw::ConvexShape(std::span<const RE::NiPoint3> a_vertices, const DebugStyle& a_style) {

		if (a_vertices.size() < 4 || !Accepting()) {
			return;
		}

		//Deduplicate first. The controller shapes ship with coincident vertices, and a duplicate
		//inside a band throws off the angular ordering.
		absl::InlinedVector<RE::NiPoint3, 32> verts;

		for (const RE::NiPoint3& v : a_vertices) {

			const bool duplicate = std::ranges::any_of(verts, [&](const RE::NiPoint3& a_seen) {
				return a_seen.GetSquaredDistance(v) < kWeldEpsilon * kWeldEpsilon;
			});

			if (!duplicate) {
				verts.push_back(v);
			}
		}

		if (verts.size() < 4) {
			Hull(a_vertices, a_style);
			return;
		}

		std::ranges::sort(verts, {}, &RE::NiPoint3::z);

		//A band tolerance proportional to the shape's height keeps this scale independent.
		const float height = verts.back().z - verts.front().z;
		const float tolerance = std::max(kMinBandTolerance, height * kBandFraction);

		absl::InlinedVector<std::pair<std::size_t, std::size_t>, 8> bands;
		std::size_t first = 0;

		for (std::size_t i = 1; i <= verts.size(); ++i) {
			if (i == verts.size() || verts[i].z - verts[first].z > tolerance) {
				bands.emplace_back(first, i);
				first = i;
			}
		}

		if (bands.size() < 2) {
			Hull(a_vertices, a_style);
			return;
		}

		//One shared axis for every band, so the rings stay in phase with each other and the verticals do not cross.
		RE::NiPoint3 centroid{};

		for (const RE::NiPoint3& v : verts) {
			centroid += v;
		}

		centroid /= static_cast<float>(verts.size());

		const auto angleOf = [&centroid](const RE::NiPoint3& a_v) {
			return std::atan2(a_v.y - centroid.y, a_v.x - centroid.x);
		};

		for (const auto& [begin, end] : bands) {
			std::sort(verts.begin() + begin, verts.begin() + end, [&](const RE::NiPoint3& a_lhs, const RE::NiPoint3& a_rhs) {
				return angleOf(a_lhs) < angleOf(a_rhs);
			});
		}

		//Rings.
		for (const auto& [begin, end] : bands) {
			if (end - begin >= 3) {
				Push(std::span(verts.data() + begin, end - begin), true, a_style);
			}
		}

		//Joins between consecutive bands. A band of one or two vertices is an end cap, so it
		//spokes to every vertex of its neighbour; otherwise each vertex pairs with the closest
		//one by angle, which is what keeps a 8-into-8 join looking like a barrel.
		DebugStyle spokeStyle = a_style;
		spokeStyle.Thickness = a_style.Thickness * kSpokeThicknessScale;

		for (std::size_t b = 0; b + 1 < bands.size(); ++b) {

			const auto [lowBegin, lowEnd] = bands[b];
			const auto [highBegin, highEnd] = bands[b + 1];

			const std::size_t lowCount = lowEnd - lowBegin;
			const std::size_t highCount = highEnd - highBegin;

			if (lowCount <= 2 || highCount <= 2) {

				for (std::size_t i = lowBegin; i < lowEnd; ++i) {
					for (std::size_t j = highBegin; j < highEnd; ++j) {
						Line(verts[i], verts[j], spokeStyle);
					}
				}

				continue;
			}

			for (std::size_t i = lowBegin; i < lowEnd; ++i) {

				const float target = angleOf(verts[i]);
				std::size_t best = highBegin;
				float bestDelta = std::numeric_limits<float>::max();

				for (std::size_t j = highBegin; j < highEnd; ++j) {

					float delta = std::fabs(angleOf(verts[j]) - target);

					if (delta > kPi) {
						delta = kTwoPi - delta;
					}

					if (delta < bestDelta) {
						bestDelta = delta;
						best = j;
					}
				}

				Line(verts[i], verts[best], a_style);
			}
		}
	}

	// -------------------------------------------------------------------------
	// Node helpers
	// -------------------------------------------------------------------------

	void DebugDraw::Node(RE::NiAVObject* a_node, const DebugStyle& a_style, float a_axisLength) {

		if (!a_node || !Accepting()) {
			return;
		}

		PushMarker(a_node->world.translate, 3.0f, a_style, true);

		if (a_axisLength > 0.0f) {
			Axes(a_node->world, a_axisLength, a_style.Thickness, a_style.LifetimeMs);
		}
	}

	void DebugDraw::Skeleton(RE::NiAVObject* a_root, const DebugStyle& a_style, const SkeletonOptions& a_options) {

		if (!a_root || !Accepting()) {
			return;
		}

		SkeletonOptions options = a_options;
		options.MaxDepth = std::max(options.MaxDepth, 1);

		WalkSkeleton(a_root, a_style, options, 0);
	}

	// -------------------------------------------------------------------------
	// Buffer plumbing
	// -------------------------------------------------------------------------

	void DebugDraw::Merge(Buffer& a_dst, Buffer& a_src) {

		if (a_src.Commands.empty()) {
			return;
		}

		const auto pointBase = static_cast<std::uint32_t>(a_dst.Points.size());
		const auto stringBase = static_cast<std::uint32_t>(a_dst.Strings.size());

		a_dst.Points.insert(a_dst.Points.end(), a_src.Points.begin(), a_src.Points.end());
		a_dst.Strings.insert(a_dst.Strings.end(), a_src.Strings.begin(), a_src.Strings.end());
		a_dst.Commands.reserve(a_dst.Commands.size() + a_src.Commands.size());

		for (Command cmd : a_src.Commands) {
			cmd.First += pointBase;
			cmd.StrFirst += stringBase;
			a_dst.Commands.push_back(cmd);
		}
	}

	template <typename Pred>
	void DebugDraw::Compact(Pred a_keep) {

		m_scratch.Clear();

		for (const Command& cmd : m_active.Commands) {

			if (!a_keep(cmd)) {
				continue;
			}

			Command out = cmd;
			out.First = static_cast<std::uint32_t>(m_scratch.Points.size());

			const auto pointBegin = m_active.Points.begin() + cmd.First;
			m_scratch.Points.insert(m_scratch.Points.end(), pointBegin, pointBegin + cmd.Count);

			if (cmd.Type == Kind::kText && cmd.StrLen > 0) {
				out.StrFirst = static_cast<std::uint32_t>(m_scratch.Strings.size());
				const auto stringBegin = m_active.Strings.begin() + cmd.StrFirst;
				m_scratch.Strings.insert(m_scratch.Strings.end(), stringBegin, stringBegin + cmd.StrLen);
			}

			m_scratch.Commands.push_back(out);
		}

		std::swap(m_active.Commands, m_scratch.Commands);
		std::swap(m_active.Points, m_scratch.Points);
		std::swap(m_active.Strings, m_scratch.Strings);

		m_stats.Live = static_cast<std::uint32_t>(m_active.Commands.size());
		m_activeCount.store(m_stats.Live, std::memory_order_relaxed);
	}

	void DebugDraw::Retire(std::uint64_t a_nowMs) {

		Compact([a_nowMs](const Command& a_cmd) {
			return a_cmd.Expiry != 0ull && a_cmd.Expiry > a_nowMs;
		});

		std::uint32_t tracked = 0;

		for (const Command& cmd : m_active.Commands) {
			tracked += cmd.Key != 0ull ? 1u : 0u;
		}

		m_stats.Timed = m_stats.Live - tracked;
		m_stats.Tracked = tracked;
	}

	// -------------------------------------------------------------------------
	// Render hooks
	// -------------------------------------------------------------------------

	std::uint32_t DebugDraw::AddRenderHook(RenderHook a_hook) {

		if (!a_hook) {
			return 0u;
		}

		auto& self = GetSingleton();
		const std::lock_guard lock{ self.m_hookLock };

		const std::uint32_t id = ++self.m_nextHookId;
		self.m_renderHooks.emplace_back(id, std::move(a_hook));

		return id;
	}

	void DebugDraw::RemoveRenderHook(std::uint32_t a_id) {

		if (a_id == 0u) {
			return;
		}

		auto& self = GetSingleton();
		const std::lock_guard lock{ self.m_hookLock };

		std::erase_if(self.m_renderHooks, [a_id](const auto& a_entry) {
			return a_entry.first == a_id;
		});
	}

	void DebugDraw::InvokeRenderHooks() {

		//Copied out so a hook is free to register or remove one without deadlocking on m_hookLock.
		std::vector<std::pair<std::uint32_t, RenderHook>> hooks;

		{
			const std::lock_guard lock{ m_hookLock };

			if (m_renderHooks.empty()) {
				return;
			}

			hooks = m_renderHooks;
		}

		for (const auto& hook : hooks | std::views::values) {
			if (hook) {
				hook();
			}
		}
	}

	// -------------------------------------------------------------------------
	// Render
	// -------------------------------------------------------------------------

	void DebugDraw::Render() {

		GTS_PROFILE_SCOPE("DebugDraw Render");

		const auto renderStart = std::chrono::steady_clock::now();
		const std::uint64_t now = NowMs();

		const auto& settings = Config::Advanced.Overlay;
		const bool enabled = Active();
		const bool frozen = IsFrozen();

		//Before the staging swap, so whatever the hooks submit is drawn this frame rather than next.
		if (enabled && !frozen) {
			InvokeRenderHooks();
		}

		{
			const std::lock_guard lock{ m_lock };
			m_stats.ResetFrame();

			if (frozen) {
				m_staging.Clear();
			}
			else {
				std::swap(m_staging, m_pending);
			}
		}

		if (!frozen) {
			EvictReplacedGroups();
			Merge(m_active, m_pending);
			m_pending.Clear();
		}

		if (!enabled) {

			if (!frozen) {
				m_active.Clear();
				m_stats.Live = 0;
				m_stats.Timed = 0;
				m_stats.Tracked = 0;
				m_activeCount.store(0, std::memory_order_relaxed);
			}

			return;
		}

		if (settings.bHideWhenPaused) {
			if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
				if (!frozen) {
					Retire(now);
				}
				return;
			}
		}

		ImGuiViewport* viewport = ImGui::GetMainViewport();

		if (!viewport || !m_projector.Begin(viewport->Pos, viewport->Size)) {
			if (!frozen) {
				Retire(now);
			}
			return;
		}

		//Drawing is supposed to happen in the backround
		ImDrawList* list = ImGui::GetBackgroundDrawList();

		if (!list) {
			if (!frozen) {
				Retire(now);
			}
			return;
		}

		//Clip against a slightly bigger rect so joins just off screen still bend correctly
		m_clipMin = ImVec2(viewport->Pos.x - kClipMargin, viewport->Pos.y - kClipMargin);
		m_clipMax = ImVec2(viewport->Pos.x + viewport->Size.x + kClipMargin, viewport->Pos.y + viewport->Size.y + kClipMargin);

		const float globalAlpha = std::clamp(settings.fGlobalAlpha, 0.0f, 1.0f);
		const float thicknessScale = std::max(settings.fThicknessScale, 0.05f);
		const float fadeSpan = settings.fFadeEndDist - settings.fFadeStartDist;
		const RE::NiPoint3 cameraPos = m_projector.CameraPos();

		for (const Command& cmd : m_active.Commands) {

			//Midpoint of the first and last vertex.
			const RE::NiPoint3& first = m_active.Points[cmd.First];
			const RE::NiPoint3& last = m_active.Points[cmd.First + cmd.Count - 1u];
			const float distance = ((first + last) * 0.5f - cameraPos).Length();

			if (settings.fMaxDrawDist > 0.0f && distance > settings.fMaxDrawDist) {
				++m_stats.CulledDistance;
				continue;
			}

			float alpha = globalAlpha;

			if (settings.bDepthFade && fadeSpan > 1.0f) {
				const float t = std::clamp((distance - settings.fFadeStartDist) / fadeSpan, 0.0f, 1.0f);
				alpha *= 1.0f - t;
			}

			//Below half a quantisation step there is nothing left to draw
			if (alpha <= 1.0f / 512.0f) {
				++m_stats.CulledDistance;
				continue;
			}

			const ImU32 color = ImUtil::Colors::WithAlpha(cmd.Color, alpha);

			switch (cmd.Type) {

				case Kind::kPolyline:
					RenderPolyline(list, cmd, color, cmd.Thickness * thicknessScale);
					break;

				case Kind::kMarker:
					RenderMarker(list, cmd, color);
					break;

				case Kind::kText:
					RenderText(list, cmd, color);
					break;
			}
		}

		m_stats.Projections = m_projector.Projections();
		m_stats.PointsUsed = static_cast<std::uint32_t>(m_active.Points.size());

		if (settings.bShowStats) {
			RenderStatsOverlay(list);
		}

		if (!frozen) {
			Retire(now);
		}

		m_stats.PushSample(std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - renderStart).count());
	}

	void DebugDraw::FlushRun(ImDrawList* a_list, ImU32 a_col, float a_thickness) {

		if (m_run.size() >= 2) {
			a_list->AddPolyline(m_run.data(), static_cast<int>(m_run.size()), a_col, ImDrawFlags_None, a_thickness);
			++m_stats.Batches;
		}

		m_run.clear();
	}

	bool DebugDraw::ClipSegment(ImVec2& a_from, ImVec2& a_to) const {

		const float dx = a_to.x - a_from.x;
		const float dy = a_to.y - a_from.y;

		const std::array<float, 4> p{ -dx, dx, -dy, dy };
		const std::array<float, 4> q{
			a_from.x - m_clipMin.x,
			m_clipMax.x - a_from.x,
			a_from.y - m_clipMin.y,
			m_clipMax.y - a_from.y
		};

		float t0 = 0.0f;
		float t1 = 1.0f;

		for (std::size_t i = 0; i < 4; ++i) {

			if (p[i] == 0.0f) {
				if (q[i] < 0.0f) {
					return false;
				}
				continue;
			}

			const float t = q[i] / p[i];

			if (p[i] < 0.0f) {
				if (t > t1) return false;
				if (t > t0) t0 = t;
			}
			else {
				if (t < t0) return false;
				if (t < t1) t1 = t;
			}
		}

		const ImVec2 origin = a_from;
		a_from = ImVec2(origin.x + t0 * dx, origin.y + t0 * dy);
		a_to = ImVec2(origin.x + t1 * dx, origin.y + t1 * dy);
		return true;
	}

	void DebugDraw::RenderPolyline(ImDrawList* a_list, const Command& a_cmd, ImU32 a_col, float a_thickness) {

		const std::uint32_t count = a_cmd.Count;
		const RE::NiPoint3* world = m_active.Points.data() + a_cmd.First;

		m_projected.resize(count);
		m_valid.resize(count);

		std::uint32_t visible = 0;

		for (std::uint32_t i = 0; i < count; ++i) {
			const bool ok = m_projector.Project(world[i], m_projected[i]);
			m_valid[i] = ok ? 1u : 0u;
			visible += ok ? 1u : 0u;
		}

		if (visible == 0u) {
			++m_stats.CulledBehind;
			return;
		}

		const float thickness = std::max(a_thickness, 0.5f);
		const std::uint32_t segments = a_cmd.Closed ? count : count - 1u;

		m_run.clear();

		for (std::uint32_t s = 0; s < segments; ++s) {

			const std::uint32_t i0 = s;
			const std::uint32_t i1 = (s + 1u) % count;

			if (!m_valid[i0] && !m_valid[i1]) {
				FlushRun(a_list, a_col, thickness);
				continue;
			}

			ImVec2 from = m_projected[i0];
			ImVec2 to = m_projected[i1];

			//Exactly one end is behind the near plane, cut the segment at the plane instead of
			//dropping it, so a line running past the camera still draws the part you can see.
			if (!m_valid[i0]) {
				const RE::NiPoint3 clipped = m_projector.ClipToNearPlane(world[i1], world[i0]);
				if (!m_projector.Project(clipped, from)) {
					FlushRun(a_list, a_col, thickness);
					continue;
				}
			}
			else if (!m_valid[i1]) {
				const RE::NiPoint3 clipped = m_projector.ClipToNearPlane(world[i0], world[i1]);
				if (!m_projector.Project(clipped, to)) {
					FlushRun(a_list, a_col, thickness);
					continue;
				}
			}

			if (!ClipSegment(from, to)) {
				++m_stats.CulledOffscreen;
				FlushRun(a_list, a_col, thickness);
				continue;
			}

			//Extend the open run when this segment picks up where the last one stopped. That is the
			//common case, and it collapses a whole ring into a single AddPolyline.
			if (!m_run.empty() && NearlyEqual(m_run.back(), from)) {
				m_run.push_back(to);
			}
			else {
				FlushRun(a_list, a_col, thickness);
				m_run.push_back(from);
				m_run.push_back(to);
			}

			++m_stats.Segments;
		}

		FlushRun(a_list, a_col, thickness);
	}

	void DebugDraw::RenderMarker(ImDrawList* a_list, const Command& a_cmd, ImU32 a_col) {

		ImVec2 pos;

		if (!m_projector.Project(m_active.Points[a_cmd.First], pos)) {
			++m_stats.CulledBehind;
			return;
		}

		if (pos.x < m_clipMin.x || pos.x > m_clipMax.x || pos.y < m_clipMin.y || pos.y > m_clipMax.y) {
			++m_stats.CulledOffscreen;
			return;
		}

		const float radius = std::max(a_cmd.Radius, 1.0f);

		if (a_cmd.Filled) {
			a_list->AddCircleFilled(pos, radius, a_col);
		}
		else {
			a_list->AddCircle(pos, radius, a_col, 0, std::max(a_cmd.Thickness, 1.0f));
		}

		++m_stats.Batches;
	}

	void DebugDraw::RenderText(ImDrawList* a_list, const Command& a_cmd, ImU32 a_col) {

		ImVec2 pos;

		if (!m_projector.Project(m_active.Points[a_cmd.First], pos)) {
			++m_stats.CulledBehind;
			return;
		}

		const char* begin = m_active.Strings.data() + a_cmd.StrFirst;
		const char* end = begin + a_cmd.StrLen;

		const float baseSize = std::max(ImGui::GetFontSize(), 1.0f);
		const float size = std::max(baseSize * Config::Advanced.Overlay.fTextScale, 4.0f);
		const float scale = size / baseSize;

		const ImVec2 extent = ImGui::CalcTextSize(begin, end);
		const ImVec2 scaled(extent.x * scale, extent.y * scale);

		if (a_cmd.Centered) {
			pos.x -= scaled.x * 0.5f;
			pos.y -= scaled.y * 0.5f;
		}

		if (pos.x + scaled.x < m_clipMin.x || pos.x > m_clipMax.x || pos.y + scaled.y < m_clipMin.y || pos.y > m_clipMax.y) {
			++m_stats.CulledOffscreen;
			return;
		}

		//Added for contrast
		const ImU32 shadow = IM_COL32(0, 0, 0, ImUtil::Colors::AlphaOf(a_col));

		a_list->AddText(nullptr, size, ImVec2(pos.x + 1.0f, pos.y + 1.0f), shadow, begin, end);
		a_list->AddText(nullptr, size, pos, a_col, begin, end);

		m_stats.Batches += 2u;
	}

	void DebugDraw::RenderStatsOverlay(ImDrawList* a_list) const {

		const std::string text = std::format(
			"Debug Overlay\n"
			"Live:\t\t{:>6}  (Timed {}, Tracked {})\n"
			"Submitted:\t\t{:>6}  Dropped {}\n"
			"Batches:\t\t{:>6}  Segments {}\n"
			"Projected:\t\t{:>6}  Verts {}\n"
			"Culled:\t\tbehind {} / Off {} / Dist {}\n"
			"Render:\t\t{:.3f} ms",
			m_stats.Live, m_stats.Timed, m_stats.Tracked,
			m_stats.Submitted, m_stats.Dropped,
			m_stats.Batches, m_stats.Segments,
			m_stats.Projections, m_stats.PointsUsed,
			m_stats.CulledBehind, m_stats.CulledOffscreen, m_stats.CulledDistance,
			m_stats.RenderMs);

		const ImVec2 origin(m_projector.ViewportPos().x + 12.0f, m_projector.ViewportPos().y + 12.0f);
		const ImVec2 extent = ImGui::CalcTextSize(text.c_str(), text.c_str() + text.size());

		a_list->AddRectFilled(
			ImVec2(origin.x - 6.0f, origin.y - 4.0f),
			ImVec2(origin.x + extent.x + 6.0f, origin.y + extent.y + 4.0f),
			IM_COL32(0, 0, 0, 150),
			4.0f
		);

		a_list->AddText(origin, DebugCol::White, text.c_str(), text.c_str() + text.size());
	}
}
