
#define JED_TASK_IMPLEMENTATION
#include "task/task.hpp"

int main()
{
	tsk::CreateWorkerThreads();

	auto task = tsk::CreateTask([](size_t thread_id, tsk::TaskId this_task, void* task_data)
	{
		int (*data)[10] = tsk::GetData<int[10]>(task_data);
		// could also use auto data = tsk::GetData<int[10]>(task_data); or auto *data = tsk::GetData<int[10]>(task_data);

		for(size_t i = 0; i < 10; ++i)
		{
			(*data)[i] = 1;
		}
	});

	int my_array[10];
	my_array[9] = 0;

	//tsk::StoreData(task, my_array); // Error int[10] too large to store, must store pointer to my_array
	tsk::StoreData(task, &my_array);
	tsk::AsyncRun(task);
	tsk::Wait(task);

	for(size_t i = 0; i < 10; ++i)
	{
		assert(my_array[i] == 1 && "Error: Didn't increment value in array correctly");
	}

	tsk::StopWorkerThreads();
}