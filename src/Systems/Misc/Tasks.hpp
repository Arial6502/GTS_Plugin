#pragma once

namespace GTS {

	enum class UpdateKind {
		Main,
		Camera,
		Havok,
		PostPhysics,
	};

	struct OneshotUpdate {
		double timeToLive;
	};

	class BaseTask {
		public:
		virtual ~BaseTask() = default;
		virtual bool Update() = 0;

		__forceinline UpdateKind UpdateOn() const {
			return this->updateOnKind;
		}

		__forceinline void SetUpdateOn(UpdateKind updateOn) {
			this->updateOnKind = updateOn;
		}

		#ifdef GTS_PROFILER_ENABLED

		// Where this task was spawned from.
		void SetOrigin(const std::source_location& a_where) {
			m_origin = a_where;
		}

		[[nodiscard]] std::uint32_t ProfilerSlot();

		#endif

		protected:
		UpdateKind updateOnKind = UpdateKind::Main;

		#ifdef GTS_PROFILER_ENABLED
		std::source_location m_origin {};
		std::uint32_t m_profilerSlot = kInvalidSlot;
		bool m_slotResolved = false;
		#endif
	};

	class Oneshot : public BaseTask {
		public:
		Oneshot(const std::function<void(const OneshotUpdate&)>& tasking);

		virtual bool Update() override;

		private:
		double creationTime = 0.0;
		std::function<void(const OneshotUpdate&)> tasking;
	};

	struct TaskUpdate {
		double runtime;
		double delta;
	};

	class Task : public BaseTask {
		public:
		Task(const std::function<bool(const TaskUpdate&)>& tasking);

		virtual bool Update() override;

		private:
		bool initRun = false;
		double startTime = 0.0;
		double lastRunTime = 0.0;
		std::function<bool(const TaskUpdate&)> tasking;
	};

	struct TaskForUpdate {
		double runtime;         // Total runtime in seconds
		double delta;           // Time delta since last runtime
		double progress;        // How close to completion on a scale of 0.0...1.0
		double progressDelta;   // How much progress has been gained since last time
	};

	// A `TaskFor` runs until it returns false OR the duration has elapsed
	class TaskFor : public BaseTask {
		public:
		TaskFor(double duration, const std::function<bool(const TaskForUpdate&)>& tasking);

		virtual bool Update() override;

		private:
		bool initRun = false;
		double startTime = 0.0;
		double lastRunTime = 0.0;
		double lastProgress = 0.0;
		std::function<bool(const TaskForUpdate&)> tasking;
		double duration;
	};

	class TaskManager : public EventListener, public CInitSingleton<TaskManager> {

		public:
		virtual void OnMainUpdate() override;
		virtual void OnCameraUpdate() override;
		virtual void OnHavokUpdate() override;
		virtual void OnPostSMPUpdate() override;
		virtual void OnPluginReset() override;
		virtual void OnSerdePostLoad() override;

		static void ChangeUpdate(std::string_view name, UpdateKind updateOn);
		static void Cancel(std::string_view name);

		// The trailing source_location is filled in by the compiler at each call site, so every
		// task carries where it came from at no cost to the caller.
		static void Run(const std::function<bool(const TaskUpdate&)>& tasking, std::source_location where = std::source_location::current());
		static void Run(std::string_view name, const std::function<bool(const TaskUpdate&)>& tasking, std::source_location where = std::source_location::current());

		static void RunFor(float duration, const std::function<bool(const TaskForUpdate&)>& tasking, std::source_location where = std::source_location::current());
		static void RunFor(std::string_view name, float duration, const std::function<bool(const TaskForUpdate&)>& tasking, std::source_location where = std::source_location::current());

		static void RunOnce(const std::function<void(const OneshotUpdate&)>& tasking, std::source_location where = std::source_location::current());
		static void RunOnce(std::string_view name, const std::function<void(const OneshotUpdate&)>& tasking, std::source_location where = std::source_location::current());

		static void CancelAllTasks();

		[[nodiscard]] static std::size_t LiveTaskCount();

		private:
		static void RunQueued(UpdateKind kind);
		static std::string GenerateName(void* ptr);
		static inline absl::flat_hash_map<std::string, std::shared_ptr<BaseTask>> m_taskings;
		static inline std::mutex m_taskingsLock;
	};
}