#define JED_TASK_IMPLEMENTATION
#include "task/task.hpp"

int main()
{
	tsk::CreateWorkerThreads();

	auto task = tsk::CreateTask([](size_t thread_id, tsk::TaskId this_task, void* task_data)
	{
		auto data = tsk::GetData<int>(task_data);

		data++;

		assert(data == 1 && "Error: Didn't increment the value of data inside the task correctly");
	});

	int a = 0;
	tsk::StoreData(task, a);
	tsk::AsyncRun(task);
	tsk::Wait(task);

	assert(a == 0 && "Error: Incrementing the value of the stored data shouldn't increment the value outside of the task");

	tsk::StopWorkerThreads();
}