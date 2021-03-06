
#define JED_TASK_IMPLEMENTATION
#include "task/task.hpp"

#include <iostream>

int main()
{
	tsk::CreateWorkerThreads();

	auto task = tsk::CreateTask([](size_t thread_id, tsk::TaskId this_task, void* task_data)
	{
		std::cout << "Prints from inside the task\n";
	});

	std::cout <<"Prints before the task runs\n";

	tsk::AsyncRun(task);
	tsk::Wait(task);

	std::cout <<"Prints after the task runs\n";

	tsk::StopWorkerThreads();
}