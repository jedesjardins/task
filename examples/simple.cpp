
#define JED_TASK_IMPLEMENTATION
#include "task.hpp"

int main()
{
	tsk::CreateWorkerThreads();

	auto add_task = tsk::CreateTask([](size_t thread_id, tsk::TaskId this_task, void* task_data)
	{
		auto data = tsk::GetData<int*>(task_data);

		(*data)++;
	});

	int a = 0;
	tsk::StoreData(add_task, &a);
	tsk::AsyncRun(add_task);
	tsk::Wait(add_task);

	assert(a == 1 && "Add thread didn't work correctly");

	tsk::StopWorkerThreads();
}