
#define JED_TASK_IMPLEMENTATION
#include "task/task.hpp"

int main()
{
	tsk::CreateWorkerThreads();

	auto task = tsk::CreateTask([](size_t thread_id, tsk::TaskId this_task, void* task_data)
	{
		auto data = tsk::GetData<int*>(task_data);

		(*data)++;
	});

	int a = 0;
	tsk::StoreData(task, &a);
	tsk::AsyncRun(task);
	tsk::Wait(task);

	assert(a == 1 && "Error: Incrementing the value of the stored pointer didn't increment the value outside of the task");

	tsk::StopWorkerThreads();
}