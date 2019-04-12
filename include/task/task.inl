

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
TaskId CreateParallelForTask(T* data, size_t count, ParallelTaskFunction function, SplitFunction splitter)
{
	typedef parallel_for_task_data<T> TaskData;

	auto task_data = new TaskData{data, count, function, splitter};

	TaskId task = CreateTask(&ParallelForFunction<TaskData>);
	StoreData(task, task_data);

	return task;
}