#pragma once

#include <array>
#include <atomic>

/*
*  Atomic int based circular queue implementation
*  Should be used in "single producer single consumer" scenario
*/
template<class T>
struct LockFreeCircleQueue
{
	LockFreeCircleQueue(size_t _size) : head(0), tail(0),size(_size){
		buffer = new T[size];
	}

	bool enqueue(const T& value)
	{
		size_t currentTail = tail.load(std::memory_order_relaxed);
		size_t nextTail = (currentTail + 1) % size;
		if (nextTail == head.load(std::memory_order_acquire)) {
			return false;
		}

		buffer[currentTail] = value;
		tail.store(nextTail, std::memory_order_release);
		return true;
	}

	void waitAndEnqueue(const T& value)
	{
		while (!enqueue(value));
	}

	bool dequeue(T* value)
	{
		size_t currentHead = head.load(std::memory_order_relaxed);
		if (currentHead == tail.load(std::memory_order_acquire)) {
			return false;
		}

		*value = buffer[currentHead];
		head.store((currentHead + 1) % size, std::memory_order_release);
		return true;
	}

	T waitAndDequeue()
	{
		T val;
		while (!dequeue(&val));
		return val;
	}

	~LockFreeCircleQueue()
	{
		delete[] buffer;
	}
private:
	T* buffer;
	std::atomic<size_t> head;
	std::atomic<size_t> tail;
	size_t size;
};