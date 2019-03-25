#pragma once

#include <iostream>
#include <thread>
#include <atomic>
#include <algorithm>
#include <iterator>
#include <exception>
#include <numeric>
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
		const size_t blockSize = length / (amountOfThreads + 1);

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

		try
		{
			for (size_t i = 0; i < amountOfThreads; ++i)
				futures[i].get();
		}
		catch (std::exception const& e)
		{
			std::cerr << e.what() << std::endl;
		}
		catch (...)
		{
			std::cerr << "Unknown exception" << std::endl;
		}
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

		const size_t amountOfThreads = std::min(maxThreads, static_cast<size_t>(length / minPerThread));
		const size_t blockSize = length / (amountOfThreads + 1);

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
				std::advance(blockEnd, blockSize);
				threads[i] = std::thread(findElement(), blockStart, blockEnd, match, &result, &doneFlag);

				blockStart = blockEnd;
			}

			findElement()(blockStart, last, match, &result, &doneFlag);
		}

		if (!doneFlag.load())
			return last;

		return result.get_future().get();

	}

	// -------------------------------------------------------------------------------------------------------------
	// std::partial_sum with using concurrency 

	template<typename Iterator>
	void partial_sum(Iterator first, Iterator last)
	{
		using valueType = typename std::iterator_traits<Iterator>::value_type;

		struct processChunk
		{
			void operator()(Iterator begin, Iterator end,
				std::future<valueType>* prevEndValue, std::promise<valueType>* endValue)
			{
				try
				{
					auto tempEnd = end;
					++tempEnd;
					std::partial_sum(begin, tempEnd, begin);
					if (prevEndValue)
					{
						const valueType addend = prevEndValue->get();
					
						std::for_each(begin, tempEnd, [addend](valueType& item)
						{
							item += addend;
						});

						if (endValue)
							endValue->set_value(*end);
						

					}
					else if (endValue)
						endValue->set_value(*end);

				} 

				catch (...)
				{
					if (endValue)
						endValue->set_exception(std::current_exception());
					else
						throw;
				}
			}
		};

		const typename std::iterator_traits<Iterator>::difference_type length = std::distance(first, last);

		if (!length)
			return;

		const size_t hardwareThreads = std::thread::hardware_concurrency();
		const size_t maxThreads = hardwareThreads ? hardwareThreads : 2;

		const size_t amountOfThreads = std::min(maxThreads, static_cast<size_t>(length / minPerThread));
		const size_t blockSize = length / (amountOfThreads + 1);

		std::vector<std::thread> threads(amountOfThreads - 1);
		std::vector<std::promise<valueType>> endValues(amountOfThreads - 1);
		std::vector<std::future<valueType>> prevEndValues;
		prevEndValues.reserve(amountOfThreads - 1);

		joinThreads joiner(threads);

		Iterator blockStart = first;
		size_t i;

		for(i = 0; i < amountOfThreads - 1; ++i)
		{

			Iterator blockEnd = blockStart;
			std::advance(blockEnd, blockSize - 1); 
			threads[i] = std::thread(processChunk(), blockStart, blockEnd,
				i > 0 ? &prevEndValues[i - 1] : nullptr, &endValues[i]);
			
			threads[i].join();
			blockStart = blockEnd;
			++blockStart;
			prevEndValues.emplace_back(endValues[i].get_future());
			
		}

		Iterator finalElement = blockStart;
		std::advance(finalElement, std::distance(blockStart, last) - 1);
		processChunk()(blockStart, finalElement,
			amountOfThreads ? &prevEndValues.back() : nullptr, nullptr);


	}
}
