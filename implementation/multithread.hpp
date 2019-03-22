#pragma once

#include <iostream>
#include <thread>
#include <atomic>
#include <algorithm>
#include <iterator>
#include <exception>
#include <future>
#include <vector>

namespace multithread
{
	using size_t = unsigned int;
	constexpr size_t minPerThread = 8;

	// -------------------------------------------------------------------------------------------------------------
	// for automatically joining all threads in vector

	class joinThreads
	{
	private:
		std::vector<std::thread>& threads;

	public:
		explicit joinThreads(std::vector<std::thread>& threads) :
			threads(threads) {}

		~joinThreads()
		{
			for (size_t i = 0; i < threads.size(); ++i)
				if (threads[i].joinable())
					threads[i].join();
		}
	};

	// -------------------------------------------------------------------------------------------------------------
	// std::for_each with using concurrency

	template<typename Iterator, typename Func>
	void for_each(Iterator begin, Iterator end, Func function)
	{
		const typename std::iterator_traits<Iterator>::difference_type length = std::distance(begin, end);

		if (!length)
			return;

		const size_t hardwareThreads = std::thread::hardware_concurrency();
		const size_t maxThreads = hardwareThreads ? hardwareThreads : 2;

		const size_t amountOfThreads = std::min(maxThreads, static_cast<const size_t>(length / minPerThread));
		const size_t blockSize = amountOfThreads ? length / amountOfThreads : 0;

		std::vector<std::future<void>> futures(amountOfThreads);
		std::vector<std::thread> threads(amountOfThreads);
		joinThreads joiner(threads);

		Iterator blockStart = begin;
		for (size_t i = 0; i < amountOfThreads; ++i)
		{
			Iterator blockEnd = blockStart;
			std::advance(blockEnd, blockSize);

			std::packaged_task<void(void)> task([blockStart, blockEnd, function]()
			{
				std::for_each(blockStart, blockEnd, function);
			});

			futures[i] = task.get_future();
			threads[i] = std::thread(std::move(task));

			blockStart = blockEnd;
		}

		std::for_each(blockStart, end, function);

		for (size_t i = 0; i < amountOfThreads; ++i)
			futures[i].get();
	}

	// -------------------------------------------------------------------------------------------------------------
	// std::find with using concurrency 

	template<typename Iterator, typename MatchType = typename std::iterator_traits<Iterator>::value_type>
	Iterator find(Iterator first, Iterator last, MatchType match)
	{
		struct findElement
		{
			void operator()(Iterator begin, Iterator end, MatchType match,
				std::promise<Iterator>* result, std::atomic<bool>* doneFlag)
			{
				try
				{
					for (; begin != end && !doneFlag->load(); ++begin)
						if (*begin == match)
						{
							result->set_value(begin);
							doneFlag->store(true);
							return;
						}
				}

				catch (...)
				{
					try
					{
						result->set_exception(std::current_exception());
						doneFlag->store(true);
					}
					catch (...) {}
				}
			}
		};

		const typename std::iterator_traits<Iterator>::difference_type length = std::distance(first, last);
		if (!length)
			return last;

		const size_t hardwareThreads = std::thread::hardware_concurrency();
		const size_t maxThreads = hardwareThreads ? hardwareThreads : 2;

		const size_t amountOfThreads = std::min(maxThreads, static_cast<const size_t>(length / minPerThread));
		const size_t blockSize = amountOfThreads ? amountOfThreads / length : 0;

		std::promise<Iterator> result;
		std::atomic<bool> doneFlag(false);

		std::vector<std::thread> threads(amountOfThreads);

		// for joining all threads
		{
			joinThreads joiner(threads);

			Iterator blockStart = first;
			for (size_t i = 0; i < amountOfThreads; ++i)
			{
				Iterator blockEnd = blockStart;
				std::advance(blockStart, blockSize);
				threads[i] = std::thread(findElement(), blockStart, blockEnd, match, &result, &doneFlag);

				blockStart = blockEnd;
			}

			findElement()(blockStart, last, match, &result, &doneFlag);
		}

		if (!doneFlag.load())
			return last;

		return result.get_future().get();

	}
}
