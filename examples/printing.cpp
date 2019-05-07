
#define JED_TASK_IMPLEMENTATION
#include "task/task.hpp"

#include <cstdio>

int main()
{
	tsk::CreateWorkerThreads(5);

	fprintf(stdout, "Starting\n");

	auto parent_task = tsk::CreateTask([](size_t thread_id, tsk::TaskId this_task, void* task_data)
	{
		fprintf(stdout, "In Parent\n");

		for (size_t i = 0; i < 10; ++i)
		{
			auto child_task = tsk::CreateTask([](size_t thread_id, tsk::TaskId this_task, void* task_data)
			{
				size_t i = tsk::GetData<size_t>(task_data);
				fprintf(stdout, "In Child %zu\n", i);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				fprintf(stdout, "Again, In Child %zu, from thread: %zu\n", i, thread_id);
			});

			tsk::SetThreadAffinity(child_task, i % 2);
			tsk::StoreData(child_task, i);
			tsk::SetParent(child_task, this_task);
			tsk::AsyncRun(child_task);
		}
	});

	tsk::AsyncRun(parent_task);
	tsk::Wait(parent_task);

	tsk::StopWorkerThreads();
}