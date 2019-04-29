
#define JED_TASK_IMPLEMENTATION
#include "task/task.hpp"

int main()
{
	tsk::CreateWorkerThreads();

	std::vector<int> stuff(256, 0);

	auto parallel_task = tsk::CreateParallelForTask(&stuff[0], stuff.size(),
		// function that is run in parallel
		[](size_t thread_id, tsk::TaskId this_task, void* task_data, size_t count)
		{
			// task_data here is an array of size count
			// using it as a pointer since size is not known (parallel tasks can be split)
			int* range_start = tsk::GetData<int*>(task_data);

			for (size_t i = 0; i < count; ++i)
			{
				++range_start[i];
			}
		},
		// function that decides how many times the data should be split and run
		[](size_t count)
		{
			return count > 128;
		}
	);

	tsk::AsyncRun(parallel_task);
	tsk::Wait(parallel_task);

	for (size_t i = 0; i < stuff.size(); ++i)
	{
		assert(stuff[i] == 1 && "parallel_for didn't change the data correctly");
	}

	tsk::StopWorkerThreads();
}