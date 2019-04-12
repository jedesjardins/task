
#ifndef TASK_HPP_
#define TASK_HPP_

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
	(sizeof(TaskFunction) 					// 8
		+ sizeof(TaskId) 					// 4
		+ sizeof(int8_t)					// 1
		+ sizeof(std::atomic<uint16_t>)		// 2
		+ sizeof(std::atomic<uint8_t>)		// 1
		+ 8*sizeof(TaskId));				// 8*4 = 32
											// total = 48

struct Task
{
	TaskFunction function{nullptr};
	TaskId parent{NULL_TASK};
	int8_t thread_affinity{-1};
	std::atomic<uint16_t> unfinished_tasks{0}; // max supported children is 2^16
	std::atomic<uint8_t> continuation_num{0};
	TaskId continuations[8];
	char data[TASK_PADDING_SIZE];
};

size_t GetPossibleHardwareThreadNum();

void CreateWorkerThreads(size_t thread_count = tsk::GetPossibleHardwareThreadNum()-1,
						size_t max_tasks = 128, size_t queue_size = 128);

void StopWorkerThreads();

TaskId CreateTask(TaskFunction function);

TaskId GetTaskId(Task*);
Task* GetTaskFromID(TaskId); // TODO: Should this be public?

void SetParent(TaskId this_task, TaskId parent_task);

void SetThreadAffinity(TaskId this_task, size_t destination_thread_id);

TaskId AddContinuation(TaskId ancestor, TaskId continuation);

void SyncRun(TaskId task);

void AsyncRun(TaskId task);

void Wait(TaskId task);

size_t GetThreadID();

bool DefaultSplit(size_t count);

//
// You must
// #include "tsk/task.inl"
// to use the following functions
//

template <typename T>
typename std::enable_if<std::less_equal<size_t>()(sizeof(T), TASK_PADDING_SIZE), void>::type 
StoreData(TaskId task, T const& data);

template <typename T>
typename std::enable_if<!std::less_equal<size_t>()(sizeof(T), TASK_PADDING_SIZE), void>::type
StoreData(TaskId task, T const& data);

template <typename T>
typename std::enable_if<std::less_equal<size_t>()(sizeof(T), TASK_PADDING_SIZE), T& >::type
GetData(void* task_data);

template <typename T>
typename std::enable_if<!std::less_equal<size_t>()(sizeof(T), TASK_PADDING_SIZE), T* >::type
GetData(void* task_data);


template <typename T>
struct parallel_for_task_data;

template <typename TaskData>
void ParallelForFunction(size_t thread_id, TaskId this_task, void* task_data);

template <typename T>
TaskId CreateParallelForTask(T* data, size_t count, ParallelTaskFunction function, SplitFunction splitter = DefaultSplit);



//
// INLINE FUNCTIONS
//

template <typename T>
struct parallel_for_task_data
{
	typedef T DataType;

	parallel_for_task_data(DataType* data, size_t count, ParallelTaskFunction function, SplitFunction splitter);

	DataType* data;
	size_t count;
	ParallelTaskFunction function;
	SplitFunction splitter;
};

template <typename T>
parallel_for_task_data<T>::parallel_for_task_data(parallel_for_task_data::DataType* data, size_t count, ParallelTaskFunction function, SplitFunction splitter)
:data(data),
 count(count),
 function(function),
 splitter(splitter)
{
}

template <typename T>
bool operator ==(const parallel_for_task_data<T> &a, const parallel_for_task_data<T> &b)
{

	return a.data == b.data && a.count == b.count;
}

template <typename T>
typename std::enable_if<std::less_equal<size_t>()(sizeof(T), TASK_PADDING_SIZE), void>::type
StoreData(TaskId task, T const& data)
{
	static_assert((alignof(T) <= alignof(char*)), "Alignment doesn't match");

	auto task_ptr = GetTaskFromID(task);

	memcpy(task_ptr->data, &data, sizeof(T));

	//assert((data == *reinterpret_cast<T*>(task_ptr->data)) && "Data wasn't copied into task correctly");
}

template <typename T>
typename std::enable_if<!std::less_equal<size_t>()(sizeof(T), TASK_PADDING_SIZE), void>::type
StoreData(TaskId task, T const& data)
{
	StoreData(task, &data);
}

template <typename TaskData>
void ParallelForFunction(size_t thread_id, TaskId this_task, void* task_data)
{
	auto *data = GetData<TaskData>(task_data);

	//assert(data.data && "pointer to range_data was nullptr");
	//assert(data.count && "Count was 0");
	//assert(data.function && "ParallelFunction was nullptr");
	//assert(data.splitter && "SplitFunction was nullptr");


	const SplitFunction splitter = data->splitter;

	if (splitter(data->count))
	{
		// split in two
		const size_t left_count = data->count / 2u;
		//const TaskData left_data{data->data, left_count, data->function, splitter};
		auto left_data = new TaskData{data->data, left_count, data->function, splitter};
		TaskId left = CreateTask(&ParallelForFunction<TaskData>);
		SetParent(left, this_task);
		StoreData(left, left_data);
		AsyncRun(left);

		const size_t right_count = data->count - left_count;
		//const TaskData right_data(data->data + left_count, right_count, data->function, splitter);
		auto right_data = new TaskData{data->data + left_count, right_count, data->function, splitter};
		TaskId right = CreateTask(&ParallelForFunction<TaskData>);
		SetParent(right, this_task);
		StoreData(right, right_data);
		AsyncRun(right);

		delete data;
	}
	else
	{
		// execute the function on the range of data
		(data->function)(thread_id, this_task, &data->data, data->count);

		delete data;
	}
}

template <typename T>
TaskId CreateParallelForTask(T* data, size_t count, ParallelTaskFunction function, SplitFunction splitter)
{
	typedef parallel_for_task_data<T> TaskData;

	auto task_data = new TaskData{data, count, function, splitter};

	TaskId task = CreateTask(&ParallelForFunction<TaskData>);
	StoreData(task, task_data);

	return task;
}

template <typename T>
typename std::enable_if<std::less_equal<size_t>()(sizeof(T), TASK_PADDING_SIZE), T& >::type
GetData(void* task_data)
{
	return *reinterpret_cast<T*>(task_data);
}

template <typename T>
typename std::enable_if<!std::less_equal<size_t>()(sizeof(T), TASK_PADDING_SIZE), T* >::type
GetData(void* task_data)
{
	return *reinterpret_cast<T**>(task_data);
}

}; // namespace tsk

#endif /* TASK_HPP_ */



//
// IMPLEMENTATION
//

#ifdef JED_TASK_IMPLEMENTATION

#include <atomic>

namespace utl
{

template <typename T, size_t N>
class MPSCQueue
{
public:
	void push(T const& element)
	{
		data[(++tail) % N] = element;
	}

	T* peek()
	{
		if (tail < head)
		{
			return nullptr;
		}

		return &data[head%N];
	}

	void pop()
	{
		size_t old_tail = tail;
		size_t old_head = head;

		if (old_tail >= old_head)
		{
			head++;
		}
	}

	size_t size()
	{
		return (tail-head)+1;
	}

public:
	std::atomic<size_t> head{1};
	std::atomic<size_t> tail{0};

	T data[N]{};

}; // class MPNCQueue

}; // namespace utl

#include <thread>
#include <random>
#include <iostream>
#include <sstream>
#include <chrono>

namespace tsk
{

//
// StealQueue Implementation
//

class StealQueue
{
public:
	StealQueue(size_t n_capacity)
	:m_tasks{new TaskId[n_capacity]},
	 capacity{n_capacity}
	{
	}

	~StealQueue()
	{
		delete[] m_tasks;
	}

	void Push(TaskId task)
	{
		int32_t b = m_bottom;
		m_tasks[b & (capacity-1)] = task;
		m_bottom++;
	}

	TaskId Pop()
	{
		int32_t b = --m_bottom;
		int32_t t = m_top;

		if (t <= b)
		{
			// non-empty queue
			TaskId task = m_tasks[b & (capacity-1)];
			if (t != b)
			{
				// there's still more than one item left in the queue
				return task;
			}
	 
			// this is the last item in the queue
			if (std::atomic_compare_exchange_strong(&m_top, &t, t+1))
			{

				m_bottom = t+1;
				return task;
			}
			// failed race against steal operation
			return NULL_TASK;
		}
		else
		{
			// deque was already empty
			m_bottom = 0;
			m_top = 0;
			return NULL_TASK;
		}
	}

	TaskId Steal()
	{
		int32_t t = m_top;
		int32_t b = m_bottom;
		if (t < b)
		{
			// non-empty queue
			TaskId task = m_tasks[t & (capacity-1)];
			// if m_top = t then it is valid to return task, replace m_top with t+1
			if (std::atomic_compare_exchange_strong(&m_top, &t, t+1))
			{
				return task;
			}

			return NULL_TASK;
		}
		else
		{
			// empty queue
			return NULL_TASK;
		}
	}

	size_t size()
	{
		return m_bottom - m_top;
	}

public:
	std::atomic<int32_t> m_top{0};
	std::atomic<int32_t> m_bottom{0};
	TaskId* m_tasks{nullptr};
	size_t capacity{0};
};


//
// Threading data
//


static thread_local size_t thread_id{0};
static thread_local StealQueue* this_queue{nullptr};
static size_t thread_num{0};
static bool stop_threads{false};
static StealQueue **steal_queues{nullptr};
static utl::MPSCQueue<TaskId, 10> *affine_queues{nullptr};

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

static bool HasTaskCompleted(TaskId task)
{
	auto this_task_ptr = GetTaskFromID(task);

	cv.notify_all();

	/*
	if (this_task_ptr->thread_affinity != -1)
	{
		// TODO: Change this to be a cv.notify_one() on the specific threads condition variable
		
	}
	*/

	return !this_task_ptr->unfinished_tasks;
}

static StealQueue* GetWorkerThreadQueue()
{
	return steal_queues[thread_id];
}

static utl::MPSCQueue<TaskId, 10>* GetAffineThreadQueue(size_t the_thread_id)
{
	return &affine_queues[the_thread_id];
}

static StealQueue* GetStolenThreadQueue()
{

	for (size_t i = 1; i < thread_num - 1; ++i)
	{
		if (steal_queues[(thread_id + i)%thread_num]->size() > 0)
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

static TaskId GetTask()
{
	utl::MPSCQueue<TaskId, 10> * affine_queue = GetAffineThreadQueue(thread_id);

	auto task_id_ptr = affine_queue->peek();

	if (task_id_ptr != nullptr)
	{
		affine_queue->pop();
		return *task_id_ptr;
	}

	StealQueue * queue = GetWorkerThreadQueue();

	auto task = queue->Pop();
	if (task == NULL_TASK)
	{

		StealQueue* steal_queue = GetStolenThreadQueue();
		if (!steal_queue || steal_queue == this_queue)
		{
			return NULL_TASK;
		}

		TaskId stolen_task = steal_queue->Steal();
		if (stolen_task == NULL_TASK)
		{
			return NULL_TASK;
		}

		return stolen_task;
	}

	return task;
}

static void WorkerThreadFunction(size_t this_thread_id)
{
	thread_id = this_thread_id;
	this_queue = steal_queues[thread_id];
	task_allocator = &memory_pool[thread_id*max_task_count];

	while (!stop_threads)
	{
		TaskId task = GetTask();
		if (task != NULL_TASK)
		{
			SyncRun(task);
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
	affine_queues = new utl::MPSCQueue<TaskId, 10>[thread_num];

	//initialize Stealing Queues
	steal_queues = new StealQueue* [thread_num];
	steal_queues[0] = new StealQueue{queue_size};
	this_queue = steal_queues[0]; // main thread StealQueue
	for (size_t i = 1; i < thread_num; ++i)
	{
		steal_queues[i] = new StealQueue{queue_size};
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

	return GetTaskId(task);
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

TaskId GetTaskId(Task* task_ptr)
{
	return task_ptr - reinterpret_cast<Task*>(memory_pool);
}

Task* GetTaskFromID(TaskId task)
{
	return reinterpret_cast<Task*>(memory_pool) + task;
}

void SyncRun(TaskId task)
{
	auto task_ptr = GetTaskFromID(task);

	(task_ptr->function)(thread_id, task, task_ptr->data);

	Finish(task_ptr);
}

void AsyncRun(TaskId task)
{
	auto task_ptr = GetTaskFromID(task);
	if (task_ptr->thread_affinity != -1)
	{
		auto affine_queue = GetAffineThreadQueue(task_ptr->thread_affinity);

		affine_queue->push(task);

		size_t val = num_waiting;

		cv.notify_all();
	}
	else
	{
		StealQueue* queue = GetWorkerThreadQueue();
		queue->Push(task);
		if (num_waiting)
		{
			cv.notify_one();
		}
	}
}

void Wait(TaskId task)
{
	// wait until the job has completed. in the meantime, work on any other job.
	while (!HasTaskCompleted(task))
	{
		TaskId next_task = GetTask();
		if (next_task != NULL_TASK)
		{
			SyncRun(next_task);
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
