#ifndef CACHE_THREADPOOL_H
#define CACHE_THREADPOOL_H

#define Thread_Pool_Version "0.0.1"
#include <vector>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <type_traits>
#include <future>

namespace threadPool{
    //定义并发类型
    using concurrency_t =  std::invoke_result_t<decltype(std::thread::hardware_concurrency)>;

    //定义返回结果
    template <typename T>
    class [[nodiscard]] multi_future
    {
    public:
        multi_future(const size_t _num_futures_ = 0):futures(_num_futures_){};
        //Provides the member constant value that is equal to true, if T is the type void, const void, volatile void, or const volatile void. Otherwise, value is equal to false.
        [[nodiscard]] std::conditional_t<std::is_void_v<T>,void,std::vector<T>> get(){
            if constexpr (std::is_void_v<T>){
                for(size_t i=0;i<futures.size();++i){
                    futures[i].get();
                }
                return ;
            }else{
                std::vector<T> results(futures.size());
                for(size_t i=0;i<futures.size();++i){
                    results[i]=futures[i].get();
                }
                return results;
            }
        }


        [[nodiscard]] std::future<T>& operator[](const size_t i)
        {
            return futures[i];
        }

        void push_back(std::future<T> future)
        {
            futures.push_back(std::move(future));
        }

        [[nodiscard]] size_t size() const
        {
            return futures.size();
        }


        void wait() const//waits for the result to become available
        {
            for (size_t i = 0; i < futures.size(); ++i)
                futures[i].wait();
        }

    private:
        std::vector<std::future<T>> futures;
    };


    //定义块的访问和分配
    template <typename T1, typename T2, typename T = std::common_type_t<T1, T2>>
    class [[nodiscard]] blocks{
    public:
        blocks(const T1 first_index_, const T2 index_after_last_, const size_t num_blocks_) : first_index(static_cast<T>(first_index_)), index_after_last(static_cast<T>(index_after_last_)), num_blocks(num_blocks_)
        {
            if (index_after_last < first_index)
                std::swap(index_after_last, first_index);
            total_size = static_cast<size_t>(index_after_last - first_index);
            block_size = static_cast<size_t>(total_size / num_blocks);
            if (block_size == 0)
            {
                block_size = 1;
                num_blocks = (total_size > 1) ? total_size : 1;
            }
        }

        [[nodiscard]] T start(const size_t i) const
        {
            return static_cast<T>(i * block_size)+first_index;
        }

        [[nodiscard]] T end(const size_t i) const
        {
            return (i == num_blocks - 1) ? index_after_last : start(i+1);
        }


        [[nodiscard]] size_t get_num_blocks() const
        {
            return num_blocks;
        }


        [[nodiscard]] size_t get_total_size() const
        {
            return total_size;
        }

    private:

        size_t block_size = 0;

        T first_index = 0;

        T index_after_last = 0;

        size_t num_blocks = 0;

        size_t total_size = 0;
    };


//定义线程池
    class [[nodiscard]] thread_pool
    {
    public:

        thread_pool(const concurrency_t thread_count_ = 0) : thread_count(determine_thread_count(thread_count_)), threads(std::make_unique<std::thread[]>(determine_thread_count(thread_count_)))
        {
            create_threads();
        }

        ~thread_pool()
        {
            wait_for_tasks();
            destroy_threads();
        }


        [[nodiscard]] size_t get_tasks_queued() const
        {
            const std::scoped_lock tasks_lock(tasks_mutex);
            return tasks.size();
        }


        [[nodiscard]] size_t get_tasks_running() const
        {
            const std::scoped_lock tasks_lock(tasks_mutex);
            return tasks_running;
        }


        [[nodiscard]] size_t get_tasks_total() const
        {
            const std::scoped_lock tasks_lock(tasks_mutex);
            return tasks_running + tasks.size();
        }


        [[nodiscard]] concurrency_t get_thread_count() const
        {
            return thread_count;
        }


        [[nodiscard]] bool is_paused() const
        {
            const std::scoped_lock tasks_lock(tasks_mutex);
            return paused;
        }


        template <typename F, typename T1, typename T2, typename T = std::common_type_t<T1, T2>, typename R = std::invoke_result_t<std::decay_t<F>, T, T>>
        [[nodiscard]] multi_future<R> parallelize_loop(const T1 first_index, const T2 index_after_last, F&& loop, const size_t num_blocks = 0)
        {
            blocks blks(first_index, index_after_last, num_blocks ? num_blocks : thread_count);
            if (blks.get_total_size() > 0)
            {
                multi_future<R> mf(blks.get_num_blocks());
                for (size_t i = 0; i < blks.get_num_blocks(); ++i)
                    mf[i] = submit(std::forward<F>(loop), blks.start(i), blks.end(i));
                return mf;
            }
            else
            {
                return multi_future<R>();
            }
        }

        template <typename F, typename T, typename R = std::invoke_result_t<std::decay_t<F>, T, T>>
        [[nodiscard]] multi_future<R> parallelize_loop(const T index_after_last, F&& loop, const size_t num_blocks = 0)
        {
            return parallelize_loop(0, index_after_last, std::forward<F>(loop), num_blocks);
        }

        void pause()
        {
            const std::scoped_lock tasks_lock(tasks_mutex);
            paused = true;
        }

        void purge()
        {
            const std::scoped_lock tasks_lock(tasks_mutex);
            while (!tasks.empty())
                tasks.pop();
        }

        template <typename F, typename T1, typename T2, typename T = std::common_type_t<T1, T2>>
        void push_loop(const T1 first_index, const T2 index_after_last, F&& loop, const size_t num_blocks = 0)
        {
            blocks blks(first_index, index_after_last, num_blocks ? num_blocks : thread_count);
            if (blks.get_total_size() > 0)
            {
                for (size_t i = 0; i < blks.get_num_blocks(); ++i)
                    push_task(std::forward<F>(loop), blks.start(i), blks.end(i));
            }
        }

        template <typename F, typename T>
        void push_loop(const T index_after_last, F&& loop, const size_t num_blocks = 0)
        {
            push_loop(0, index_after_last, std::forward<F>(loop), num_blocks);
        }

        template <typename F, typename... A>
        void push_task(F&& task, A&&... args)
        {
            {
                const std::scoped_lock tasks_lock(tasks_mutex);
                tasks.push(std::bind(std::forward<F>(task), std::forward<A>(args)...)); // cppcheck-suppress ignoredReturnValue
            }
            task_available_cv.notify_one();
        }


        void reset(const concurrency_t thread_count_ = 0)
        {
            std::unique_lock tasks_lock(tasks_mutex);
            const bool was_paused = paused;
            paused = true;
            tasks_lock.unlock();
            wait_for_tasks();
            destroy_threads();
            thread_count = determine_thread_count(thread_count_);
            threads = std::make_unique<std::thread[]>(thread_count);
            paused = was_paused;
            create_threads();
        }


        template <typename F, typename... A, typename R = std::invoke_result_t<std::decay_t<F>, std::decay_t<A>...>>
        [[nodiscard]] std::future<R> submit(F&& task, A&&... args)
        {
            std::shared_ptr<std::promise<R>> task_promise = std::make_shared<std::promise<R>>();
            push_task(
                    [task_function = std::bind(std::forward<F>(task), std::forward<A>(args)...), task_promise]
                    {
                        try
                        {
                            if constexpr (std::is_void_v<R>)
                            {
                                std::invoke(task_function);
                                task_promise->set_value();
                            }
                            else
                            {
                                task_promise->set_value(std::invoke(task_function));
                            }
                        }
                        catch (...)
                        {
                            try
                            {
                                task_promise->set_exception(std::current_exception());
                            }
                            catch (...)
                            {
                            }
                        }
                    });
            return task_promise->get_future();
        }


        void unpause()
        {
            const std::scoped_lock tasks_lock(tasks_mutex);
            paused = false;
        }


        void wait_for_tasks()
        {
            std::unique_lock tasks_lock(tasks_mutex);
            waiting = true;
            tasks_done_cv.wait(tasks_lock, [this] { return !tasks_running && (paused || tasks.empty()); });
            waiting = false;
        }


        template <typename R, typename P>
        bool wait_for_tasks_duration(const std::chrono::duration<R, P>& duration)
        {
            std::unique_lock tasks_lock(tasks_mutex);
            waiting = true;
            const bool status = tasks_done_cv.wait_for(tasks_lock, duration, [this] { return !tasks_running && (paused || tasks.empty()); });
            waiting = false;
            return status;
        }

        template <typename C, typename D>
        bool wait_for_tasks_until(const std::chrono::time_point<C, D>& timeout_time)
        {
            std::unique_lock tasks_lock(tasks_mutex);
            waiting = true;
            const bool status = tasks_done_cv.wait_until(tasks_lock, timeout_time, [this] { return !tasks_running && (paused || tasks.empty()); });
            waiting = false;
            return status;
        }

    private:
        // ========================
        // Private member functions
        // ========================

        /**
         * @brief Create the threads in the pool and assign a worker to each thread.
         */
        void create_threads()
        {
            {
                //The class scoped_lock is a mutex wrapper that provides a convenient RAII-style mechanism for owning zero or more mutexes for the duration of a scoped block.
                const std::scoped_lock tasks_lock(tasks_mutex);
                workers_running = true;
            }
            for (concurrency_t i = 0; i < thread_count; ++i)
            {
                threads[i] = std::thread(&thread_pool::worker, this);//this 是一个关键字，在C++中用于指代当前对象的指针。它是一个指向当前类实例（对象）的指针，允许在类的成员函数中访问该对象的成员变量和成员函数。
            }
        }


        void destroy_threads()
        {
            {
                const std::scoped_lock tasks_lock(tasks_mutex);
                workers_running = false;
            }
            task_available_cv.notify_all();
            for (concurrency_t i = 0; i < thread_count; ++i)
            {
                threads[i].join();
            }
        }


        [[nodiscard]] concurrency_t determine_thread_count(const concurrency_t thread_count_) const
        {
            if (thread_count_ > 0)
                return thread_count_;
            else
            {
                if (std::thread::hardware_concurrency() > 0)
                    return std::thread::hardware_concurrency();
                    //If the value is not well defined or not computable, returns 0;
                else
                    return 1;
            }
        }


        void worker()
        {
            std::function<void()> task;
            while (true)
            {
                std::unique_lock tasks_lock(tasks_mutex);
                task_available_cv.wait(tasks_lock, [this] { return !tasks.empty() || !workers_running; });
                if (!workers_running)
                    break;
                if (paused)
                    continue;
                task = std::move(tasks.front());
                tasks.pop();
                ++tasks_running;
                tasks_lock.unlock();
                task();
                tasks_lock.lock();
                --tasks_running;
                if (waiting && !tasks_running && (paused || tasks.empty()))
                    tasks_done_cv.notify_all();
            }
        }


        bool paused = false;

        std::condition_variable task_available_cv = {};

        std::condition_variable tasks_done_cv = {};

        std::queue<std::function<void()>> tasks = {};

        size_t tasks_running = 0;

        mutable std::mutex tasks_mutex = {};

        concurrency_t thread_count = 0;

        std::unique_ptr<std::thread[]> threads = nullptr;

        bool waiting = false;

        bool workers_running = false;
    };



    class [[nodiscard]] synced_stream
    {
    public:

        synced_stream(std::ostream& out_stream_ = std::cout) : out_stream(out_stream_) {}


        template <typename... T>
        void print(T&&... items) const
        {
            const std::scoped_lock lock(stream_mutex);
            (out_stream << ... << std::forward<T>(items));
        }


        template <typename... T>
        void println(T&&... items)
        {
            print(std::forward<T>(items)..., '\n');
        }


        inline static std::ostream& (&endl)(std::ostream&) = static_cast<std::ostream& (&)(std::ostream&)>(std::endl);


        inline static std::ostream& (&flush)(std::ostream&) = static_cast<std::ostream& (&)(std::ostream&)>(std::flush);

    private:

        std::ostream& out_stream;

        mutable std::mutex stream_mutex = {};
    };



    class [[nodiscard]] timer
    {
    public:

        void start()
        {
            start_time = std::chrono::steady_clock::now();
        }


        void stop()
        {
            elapsed_time = std::chrono::steady_clock::now() - start_time;
        }


        [[nodiscard]] std::chrono::milliseconds::rep ms() const
        {
            return (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time)).count();
        }

    private:
        std::chrono::time_point<std::chrono::steady_clock> start_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_time = std::chrono::duration<double>::zero();
    };

}



#endif //THREAD_POOL_THREADPOOL_H


