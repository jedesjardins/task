
#define JED_TASK_IMPLEMENTATION
#include "task/task.hpp"

#include <chrono>
#include <thread>

int main()
{
	tsk::CreateWorkerThreads();

	uint32_t data[] = {0, 0};

	std::cout << "Original data: " << &data << " " << data << " " << *data << std::endl;

	auto parent_task = tsk::CreateTask([](size_t thread_id, tsk::TaskId this_task, void* task_data)
	{
		uint32_t (*data)[2] = tsk::GetData<uint32_t(*)[2]>(task_data);

		std::cout << "In parent data: " << &data << " " << data << " " << *data << std::endl;
		std::cout << (*data)[0] << std::endl;

		auto first_child_task = tsk::CreateTask([](size_t thread_id, tsk::TaskId this_task, void* task_data)
		{
			uint32_t *data = tsk::GetData<uint32_t*>(task_data);
		
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));

			std::cout << "In first child data: " << data << " " << *data << std::endl;
			*data = 1;
		});

		tsk::StoreData(first_child_task, &(*data)[0]);
		tsk::SetParent(first_child_task, this_task);
		tsk::AsyncRun(first_child_task);

		auto second_child_task = tsk::CreateTask([](size_t thread_id, tsk::TaskId this_task, void* task_data)
		{
			uint32_t *data = tsk::GetData<uint32_t*>(task_data);

			std::this_thread::sleep_for(std::chrono::milliseconds(1000));

			std::cout << "In second child data: " << data << " " << *data << std::endl;
			*data = 1;
		});

		tsk::StoreData(second_child_task, &(*data)[1]);
		tsk::SetParent(second_child_task, this_task);
		tsk::AsyncRun(second_child_task);
	});

	tsk::StoreData(parent_task, &data);
	tsk::AsyncRun(parent_task);
	tsk::Wait(parent_task);

	assert(data[0] == 1 && "child didn't change the data correctly");
	assert(data[1] == 1 && "child didn't change the data correctly");

	tsk::StopWorkerThreads();
}