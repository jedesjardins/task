
#ifndef JED_TASK_HPP_
#define JED_TASK_HPP_

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <typeinfo>
#include <functional>

namespace tsk
{

struct Task;

using TaskId = int32_t;
constexpr TaskId NULL_TASK = -1;

using TaskFunction = void (*)(size_t thread_id, TaskId this_task, void* task_data); // doesn't need task data
using ParallelTaskFunction = void (*)(size_t thread_id, TaskId this_task, void* task_data, size_t count);
using SplitFunction = bool (*)(size_t count);

constexpr size_t TASK_PADDING_SIZE = 64 -
	(sizeof(TaskFunction) 					// 8 8
		+ sizeof(TaskId) 					// 4 2 + 1 (change to uint16_t, add hasparent)
		+ sizeof(int8_t)					// 1 1
		+ sizeof(std::atomic<uint16_t>)		// 2 1 (change supported number of children to 2^8)
		+ sizeof(std::atomic<uint8_t>)		// 1 1 ()
		+ 8*sizeof(TaskId));				// 8*4 = 32 5*2 = 10
											// total = 48 new total = 8+10+4+2 = 24 = 32-24 = 8

struct Task
{
	TaskFunction function{nullptr};
	TaskId parent{NULL_TASK};
	int8_t thread_affinity{-1};
	std::atomic<uint16_t> unfinished_tasks{0}; // max supported children is 2^16
	std::atomic<uint8_t> continuation_num{0}; // does this need to be atomic? When would continuations be added in parallel?
	TaskId continuations[8];
	char data[TASK_PADDING_SIZE];
};

size_t GetPossibleHardwareThreadNum();

void CreateWorkerThreads(size_t thread_count = tsk::GetPossibleHardwareThreadNum()-1,
						size_t max_tasks = 128, size_t queue_size = 128);

void StopWorkerThreads();

TaskId CreateTask(TaskFunction function);

TaskId GetTaskID(Task*);

Task* GetTaskFromID(TaskId);

void SetParent(TaskId this_task, TaskId parent_task);

void SetThreadAffinity(TaskId this_task, size_t destination_thread_id);

TaskId AddContinuation(TaskId ancestor, TaskId continuation);

void SyncRun(TaskId task);

void AsyncRun(TaskId task);

void Wait(TaskId task);

size_t GetThreadID();

bool DefaultSplit(size_t count);

template <typename T>
typename std::enable_if<std::less_equal<size_t>()(sizeof(T), TASK_PADDING_SIZE), void>::type 
StoreData(TaskId task, T const& data);

/*
template <typename T>
typename std::enable_if<!std::less_equal<size_t>()(sizeof(T), TASK_PADDING_SIZE), void>::type
StoreData(TaskId task, T const& data);
*/

template <typename T>
typename std::enable_if<std::less_equal<size_t>()(sizeof(T), TASK_PADDING_SIZE), T& >::type
GetData(void* task_data);

template <typename T>
typename std::enable_if<!std::less_equal<size_t>()(sizeof(T), TASK_PADDING_SIZE), T* >::type
GetData(void* task_data);



template <typename T>
TaskId CreateParallelForTask(T* data, size_t count, ParallelTaskFunction function, SplitFunction splitter = DefaultSplit);

}; // namespace tsk

#endif /* TASK_TASK_HPP_ */





//
// IMPLEMENTATION
//

#ifdef JED_TASK_IMPLEMENTATION

#include <thread>
#include <random>
#include <iostream>
#include <sstream>
#include <chrono>

#include "mul_utl/mul_utl.hpp"

namespace tsk
{

//
// Threading data
//


static thread_local size_t thread_id{0};
static thread_local utl::steal_queue<Task*>* this_queue{nullptr};
static size_t thread_num{0};
static bool stop_threads{false};
static utl::steal_queue<Task*> **steal_queues{nullptr};
static utl::mpsc_s_queue<Task*, 10> *affine_queues{nullptr};

// used to signal/wake waiting threads
static std::atomic<size_t>* queue_sizes;
static std::atomic<size_t> num_waiting{0};
static std::mutex m_mutex;
static std::condition_variable cv;

// used to signal/stop the threads
static std::mutex end_mutex;
static std::condition_variable end_cv;
static std::atomic<size_t> num_ended{0};



static size_t max_task_count{0}; 					// per thread
static Task* memory_pool{nullptr};					// shared between all threads
static thread_local Task* task_allocator{nullptr};	// pointer into memory_pool for memory allocated to this
static thread_local size_t allocated_tasks;			// allocated tasks

static Task* AllocateTask()
{
	const size_t index = allocated_tasks++;
	return &task_allocator[index & (max_task_count-1)];
}


//
// Threading Functions
//

static bool HasTaskCompleted(Task* task_ptr)
{
	cv.notify_all();

	/*
	if (this_task_ptr->thread_affinity != -1)
	{
		// TODO: Change this to be a cv.notify_one() on the specific threads condition variable
		
	}
	*/

	return !task_ptr->unfinished_tasks;
}

static utl::steal_queue<Task*>* GetWorkerThreadQueue()
{
	return steal_queues[thread_id];
}

static utl::mpsc_s_queue<Task*, 10>* GetAffineThreadQueue(size_t the_thread_id)
{
	return &affine_queues[the_thread_id];
}

static utl::steal_queue<Task*>* GetStolenThreadQueue()
{

	for (size_t i = 0; i < thread_num - 1; ++i)
	{
		if (steal_queues[i]->size() > 0)
		{
			return steal_queues[i];
		}
	}

	return nullptr;
}

static void Finish(Task* task_ptr)
{
	const size_t unfinished_tasks = --(task_ptr->unfinished_tasks);

	if (unfinished_tasks == 0)
	{

		if (task_ptr->parent != NULL_TASK)
		{
			Finish(GetTaskFromID(task_ptr->parent));
		}


		// run descendent tasks
		for (size_t i = 0; i < task_ptr->continuation_num && i < 7; ++i)
		{
			AsyncRun(task_ptr->continuations[i]);
		}

		// Sychronously run chained helper continuation tasks
		if (task_ptr->continuation_num >= 8)
		{
			SyncRun(task_ptr->continuations[7]);
		}
	}
}

static Task* GetTask()
{
	//utl::mpsc_s_queue<TaskId, 10> * affine_queue = GetAffineThreadQueue(thread_id);
	auto affine_queue_ptr = GetAffineThreadQueue(thread_id);

	if (affine_queue_ptr->size() > 0) {
		auto task_ptr = affine_queue_ptr->front();
		affine_queue_ptr->pop_front();
		return task_ptr;
	}

	// pop
	auto task_ptr = this_queue->pop_front();
	if (!task_ptr)
	{

		utl::steal_queue<Task*>* steal_queue = GetStolenThreadQueue();
		if (!steal_queue || steal_queue == this_queue)
		{
			return nullptr;
		}

		// steal
		Task* stolen_task_ptr = steal_queue->pop_back();

		return stolen_task_ptr;
	}

	return task_ptr;
}

static void SyncRun(Task* task_ptr, TaskId task_id)
{
	(task_ptr->function)(thread_id, task_id, task_ptr->data);

	Finish(task_ptr);
}

static void WorkerThreadFunction(size_t this_thread_id)
{
	thread_id = this_thread_id;
	this_queue = steal_queues[thread_id];
	task_allocator = &memory_pool[thread_id*max_task_count];

	while (!stop_threads)
	{
		Task* task_ptr = GetTask();
		if (task_ptr)
		{
			SyncRun(task_ptr, GetTaskID(task_ptr));
		}
		else
		{
			++num_waiting;
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				cv.wait(lock);
			}
			--num_waiting;
		}
	}
	
	++num_ended;
	end_cv.notify_one();
}

size_t GetPossibleHardwareThreadNum()
{
	return std::thread::hardware_concurrency();
}

void CreateWorkerThreads(size_t thread_count, size_t max_tasks, size_t queue_size)
{
	//assert(thread_count < 32);
	//assert( thread_count * max_tasks doesn't overflow TaskID(int32_t))

	thread_num = thread_count+1; // has to allocate for the main thread as well
	stop_threads = false;

	//initialize affinity queues
	affine_queues = new utl::mpsc_s_queue<Task*, 10>[thread_num];

	//initialize Stealing Queues
	steal_queues = new utl::steal_queue<Task*>* [thread_num];
	steal_queues[0] = new utl::steal_queue<Task*>{queue_size};
	this_queue = steal_queues[0]; // main thread steal_queue
	for (size_t i = 1; i < thread_num; ++i)
	{
		steal_queues[i] = new utl::steal_queue<Task*>{queue_size};
	}

	//initialize Task Memory Pool
	max_task_count = max_tasks;
	memory_pool = new Task[thread_num * max_task_count];
	task_allocator = &memory_pool[0];

	for (size_t i = 1; i < thread_num; ++i)
	{
		std::thread a_thread{WorkerThreadFunction, i};
		a_thread.detach();
	}
}

void StopWorkerThreads()
{
	stop_threads = true;

	while(num_ended < thread_num - 1)
	{
		cv.notify_all();
		std::unique_lock<std::mutex> lock(end_mutex);
		end_cv.wait_for(lock, std::chrono::milliseconds(1));
	}

	for (size_t i = 0; i < thread_num - 1; ++i)
	{
		delete steal_queues[i];
	}

	delete[] memory_pool;
	delete[] steal_queues;
}

TaskId CreateTask(TaskFunction function)
{
	Task* task = AllocateTask();
	task->function = function;
	task->parent = NULL_TASK;
	task->thread_affinity = -1;
	task->unfinished_tasks = 1;
	task->continuation_num = 0;

	return GetTaskID(task);
}

void SetParent(TaskId this_task, TaskId parent_task)
{
	auto this_task_ptr = GetTaskFromID(this_task);
	auto parent_task_ptr = GetTaskFromID(parent_task);

	parent_task_ptr->unfinished_tasks++;
	this_task_ptr->parent = parent_task;
}

void SetThreadAffinity(TaskId this_task, size_t destination_thread_id)
{
	assert(destination_thread_id < thread_num);

	auto this_task_ptr = GetTaskFromID(this_task);

	this_task_ptr->thread_affinity = destination_thread_id;	
}

TaskId AddContinuation(TaskId ancestor, TaskId descendant)
{
	auto ancestor_ptr = GetTaskFromID(ancestor);

	const uint8_t count = ++(ancestor_ptr->continuation_num);

	if (count < 8)
	{
		ancestor_ptr->continuations[count - 1] = descendant;
		return ancestor;
	}
	else if (count == 8)
	{
		// chain a new empty task for more continuations
		auto chained_continuation = CreateTask([](size_t thread_id, TaskId this_task, void* task_data)
			{});

		ancestor_ptr->continuations[count - 1] = chained_continuation;

		return AddContinuation(chained_continuation, descendant);
	}
	else
	{
		// TODO: Error here
		return NULL_TASK;
	}
}

TaskId GetTaskID(Task* task_ptr)
{
	return task_ptr - reinterpret_cast<Task*>(memory_pool);
}

Task* GetTaskFromID(TaskId task)
{
	return reinterpret_cast<Task*>(memory_pool) + task;
}

void SyncRun(TaskId task_id)
{
	auto task_ptr = GetTaskFromID(task_id);

	SyncRun(task_ptr, task_id);

	//(task_ptr->function)(thread_id, task, task_ptr->data);

	//Finish(task_ptr);
}

void AsyncRun(TaskId task)
{
	auto task_ptr = GetTaskFromID(task);
	if (task_ptr->thread_affinity != -1)
	{
		auto affine_queue_ptr = GetAffineThreadQueue(task_ptr->thread_affinity);

		affine_queue_ptr->push_back(task_ptr);

		size_t val = num_waiting;

		cv.notify_all();
	}
	else
	{
		this_queue->push_front(task_ptr);
		if (num_waiting)
		{
			cv.notify_one();
		}
	}
}

void Wait(TaskId task)
{
	Task* task_ptr = GetTaskFromID(task);

	// wait until the job has completed. in the meantime, work on any other job.
	while (!HasTaskCompleted(task_ptr))
	{
		Task* next_task_ptr = GetTask();
		if (next_task_ptr)
		{
			SyncRun(next_task_ptr, GetTaskID(next_task_ptr));
		}
	}
}

size_t GetThreadID()
{
	return thread_id;
}

bool DefaultSplit(size_t count)
{
	return count > 128 ? true : false;
}

}; // namespace tsk

#endif /* JED_TASK_IMPLEMENTATION */

//
// If JED_TASK_EXCLUDE_INL is defined task.inl won't be included here and will have to be included manually
//

#ifndef JED_TASK_EXCLUDE_INL
#include "task/task.inl"
#endif
