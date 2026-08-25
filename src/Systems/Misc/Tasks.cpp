#include "Systems/Misc/Tasks.hpp"
#include "Debug/Profilers.hpp"

#ifdef GTS_PROFILER_ENABLED
#define GTS_TASK_ORIGIN(a_task, a_where) (a_task)->SetOrigin(a_where)
#else
#define GTS_TASK_ORIGIN(a_task, a_where) ((void)(a_where))
#endif

namespace {

	struct QueuedTask {
		std::string name;
		std::shared_ptr<GTS::BaseTask> task;
	};

	std::vector<QueuedTask> CollectTasksForUpdate(auto& taskings, std::mutex& taskingsLock, GTS::UpdateKind kind) {
		std::vector<QueuedTask> queued;

		{
			std::scoped_lock lock(taskingsLock);
			queued.reserve(taskings.size());

			for (auto& [name, task] : taskings) {
				if (task && task->UpdateOn() == kind) {
					queued.push_back({
						name,
						task,
					});
				}
			}
		}

		return queued;
	}

	void RemoveCompletedTasks(auto& taskings, std::mutex& taskingsLock, const std::vector<QueuedTask>& toRemove) {

		if (toRemove.empty()) {
			return;
		}

		std::scoped_lock lock(taskingsLock);

		for (const auto& queued : toRemove) {
			auto it = taskings.find(queued.name);
			if (it != taskings.end() && it->second == queued.task) {
				taskings.erase(it);
			}
		}
	}
}

namespace GTS {

	//-----------
	// TASK
	//-----------

	Task::Task(const std::function<bool(const TaskUpdate&)>& tasking) : 
		startTime(Time::WorldTimeElapsed()), lastRunTime(Time::WorldTimeElapsed()), tasking(tasking) {
			if (!this->tasking) {
				logger::info("Task constructed with empty callback!");
			}
		}

	bool Task::Update() {
		if (!this->tasking) {
			logger::info("Task::Update() Created empty Task! Address: {:X}",reinterpret_cast<uintptr_t>(this));
			return false;
		}

		auto callback = this->tasking;
		if (!callback) {
			logger::error(
				"Task callback empty! Task {:X}",
				reinterpret_cast<uintptr_t>(this)
			);
			return false;
		}
		TaskUpdate update;
		double currentTime = Time::WorldTimeElapsed();

		if (this->initRun) {
			update = TaskUpdate{
				.runtime = currentTime - this->startTime,
				.delta = currentTime - this->lastRunTime,
			};
		}
		else {
			update = TaskUpdate{
				.runtime = 0.0,
				.delta = 0.0,
			};
			this->initRun = true;
		}

		this->lastRunTime = currentTime;
		return this->tasking(update);
	}

	//-----------
	// TASK FOR
	//-----------

	TaskFor::TaskFor(double duration, const std::function<bool(const TaskForUpdate&)>& tasking)
		: startTime(Time::WorldTimeElapsed()),lastRunTime(Time::WorldTimeElapsed()), tasking(tasking), duration(duration) {}

	bool TaskFor::Update() {
		if (!this->tasking) {
			logger::info("TaskFor::Update(): Created empty Task! Address: {:X}",reinterpret_cast<uintptr_t>(this));
			return false;
		}
		double currentTime = Time::WorldTimeElapsed();
		double currentRuntime = currentTime - this->startTime;

		double currentProgress = 0.0;
		if (this->duration > 0.0) {
			currentProgress = std::clamp(currentRuntime / this->duration, 0.0, 1.0);
		}

		TaskForUpdate update;

		if (this->initRun) {
			update = TaskForUpdate{
				.runtime = currentRuntime,
				.delta = currentTime - this->lastRunTime,
				.progress = currentProgress,
				.progressDelta = currentProgress - this->lastProgress,
			};
		}
		else {
			update = TaskForUpdate{
				.runtime = 0.0,
				.delta = 0.0,
				.progress = 0.0,
				.progressDelta = 0.0,
			};
			this->initRun = true;
		}

		this->lastRunTime = currentTime;
		this->lastProgress = currentProgress;

		bool shouldContinue = this->tasking(update);
		return shouldContinue && currentRuntime <= this->duration;
	}

	//-----------
	// ONE SHOT
	//-----------

	Oneshot::Oneshot(const std::function<void(const OneshotUpdate&)>& tasking) 
		: creationTime(Time::WorldTimeElapsed()), tasking(tasking) {}

	bool Oneshot::Update() {
		if (!this->tasking) {
			logger::info("Oneshot::Update(): Created empty Task! Address: {:X}",reinterpret_cast<uintptr_t>(this));
			return false;
		}
		double currentTime = Time::WorldTimeElapsed();

		OneshotUpdate update{
			.timeToLive = currentTime - this->creationTime,
		};

		this->tasking(update);
		return false;
	}

	//---------------
	// TASK MANAGER
	//---------------

	#ifdef GTS_PROFILER_ENABLED

	std::uint32_t BaseTask::ProfilerSlot() {

		if (m_slotResolved) {
			return m_profilerSlot;
		}

		// Deferred until collection is running.
		if (!ProfilerDetail::g_Collecting.load(std::memory_order_relaxed)) {
			return kInvalidSlot;
		}

		std::string_view file = m_origin.file_name() ? m_origin.file_name() : "?";

		if (const std::size_t cut = file.find_last_of("/\\"); cut != std::string_view::npos) {
			file = file.substr(cut + 1);
		}

		m_profilerSlot = ProfilerRegistry::Register(std::format("Task {}:{}", file, m_origin.line()), SiteKind::Task, -1);
		m_slotResolved = true;
		return m_profilerSlot;
	}

	#endif

	std::string TaskManager::GenerateName(void* ptr) {
		return std::format("UNNAMED_{}", reinterpret_cast<std::uintptr_t>(ptr));
	}

	void TaskManager::RunQueued(UpdateKind kind) {

		std::vector<QueuedTask> queued = CollectTasksForUpdate(m_taskings, m_taskingsLock, kind);

		std::vector<QueuedTask> toRemove;
		toRemove.reserve(queued.size());

		for (const auto& entry : queued) {

			if (!entry.task) continue;

			#ifdef GTS_PROFILER_ENABLED
			// One row per creation site. Calls/frame on that row is the number of live instances spawned from it.
			const ScopeTimer taskScope{ entry.task->ProfilerSlot() };
			#endif

			if (!entry.task->Update()) {
				toRemove.push_back(entry);
			}
		}

		RemoveCompletedTasks(m_taskings, m_taskingsLock, toRemove);
	}

	void TaskManager::OnMainUpdate()    { RunQueued(UpdateKind::Main); }
	void TaskManager::OnCameraUpdate()  { RunQueued(UpdateKind::Camera); }
	void TaskManager::OnHavokUpdate()   { RunQueued(UpdateKind::Havok); }
	void TaskManager::OnPostSMPUpdate() { RunQueued(UpdateKind::PostPhysics); }

	std::size_t TaskManager::LiveTaskCount() {
		std::scoped_lock lock(m_taskingsLock);
		return m_taskings.size();
	}

	void TaskManager::ChangeUpdate(std::string_view name, UpdateKind updateOn) {
		std::scoped_lock lock(m_taskingsLock);

		auto it = m_taskings.find(std::string(name));
		if (it != m_taskings.end()) {
			it->second->SetUpdateOn(updateOn);
		}
	}

	void TaskManager::Cancel(std::string_view name) {
		std::scoped_lock lock(m_taskingsLock);
		m_taskings.erase(std::string(name));
	}

	void TaskManager::Run(const std::function<bool(const TaskUpdate&)>& tasking, std::source_location where) {
		auto task = std::make_shared<Task>(tasking);
		GTS_TASK_ORIGIN(task, where);
		std::string name = GenerateName(task.get());

		std::scoped_lock lock(m_taskingsLock);
		auto [it, inserted] = m_taskings.try_emplace(name, std::move(task));
		if (!inserted) {
			//logger::warn("Task '{}' already exists", name);
		}
	}

	void TaskManager::Run(std::string_view name, const std::function<bool(const TaskUpdate&)>& tasking, std::source_location where) {
		if (!tasking) {
			logger::info("TaskManager::Run empty callback: {}", name);
			return;
		}
		auto task = std::make_shared<Task>(tasking);
		GTS_TASK_ORIGIN(task, where);

		std::scoped_lock lock(m_taskingsLock);
		auto [it, inserted] = m_taskings.try_emplace(std::string(name), std::move(task));
		if (!inserted) {
			//logger::warn("Task '{}' already exists", name);
		}
	}

	void TaskManager::RunFor(float duration, const std::function<bool(const TaskForUpdate&)>& tasking, std::source_location where) {
		auto task = std::make_shared<TaskFor>(duration, tasking);
		GTS_TASK_ORIGIN(task, where);
		std::string name = GenerateName(task.get());

		std::scoped_lock lock(m_taskingsLock);
		auto [it, inserted] = m_taskings.try_emplace(name, std::move(task));
		if (!inserted) {
			//logger::warn("Task '{}' already exists", name);
		}
	}

	void TaskManager::RunFor(std::string_view name, float duration, const std::function<bool(const TaskForUpdate&)>& tasking, std::source_location where) {
		auto task = std::make_shared<TaskFor>(duration, tasking);
		GTS_TASK_ORIGIN(task, where);

		std::scoped_lock lock(m_taskingsLock);
		auto [it, inserted] = m_taskings.try_emplace(std::string(name), std::move(task));
		if (!inserted) {
			//logger::warn("Task '{}' already exists", name);
		}
	}

	void TaskManager::RunOnce(const std::function<void(const OneshotUpdate&)>& tasking, std::source_location where) {
		auto task = std::make_shared<Oneshot>(tasking);
		GTS_TASK_ORIGIN(task, where);
		std::string name = GenerateName(task.get());

		std::scoped_lock lock(m_taskingsLock);
		auto [it, inserted] = m_taskings.try_emplace(name, std::move(task));
		if (!inserted) {
			//logger::warn("Task '{}' already exists", name);
		}
	}

	void TaskManager::RunOnce(std::string_view name, const std::function<void(const OneshotUpdate&)>& tasking, std::source_location where) {
		auto task = std::make_shared<Oneshot>(tasking);
		GTS_TASK_ORIGIN(task, where);

		std::scoped_lock lock(m_taskingsLock);
		auto [it, inserted] = m_taskings.try_emplace(std::string(name), std::move(task));
		if (!inserted) {
			//logger::warn("Task '{}' already exists", name);
		}
	}

	void TaskManager::CancelAllTasks() {
		{
			std::scoped_lock lock(m_taskingsLock);
			m_taskings.clear();
		}

		logger::info("Canceled all task manager tasks");
	}
	void TaskManager::OnPluginReset() {
		CancelAllTasks();
	}
	void TaskManager::OnSerdePostLoad() {
		CancelAllTasks();
	}
}