#include "tasksys.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <syncstream>
#include <thread>
#include <vector>
#include "itasksys.h"

IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads)
    : ITaskSystem(num_threads) {}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable,
                                          int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemSerial::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads)
    : ITaskSystem(num_threads), num_threads(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {

    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    std::atomic_int num_finished{0};

    std::vector<std::thread> threads{};
    for (int i = 0; i < num_threads; i++) {

        threads.push_back(std::thread([&]() {
            while (true) {
                int num = num_finished.fetch_add(1);
                if (num > num_total_tasks) {
                    break;
                }
                runnable->runTask(num, num_total_tasks);
            }
        }));
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(
    IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(
    int num_threads)
    : ITaskSystem(num_threads),
      num_threads(num_threads),
      threads(std::vector<std::thread>{}),
      num_finished(std::atomic_int{0}),
      _runnable(nullptr),
      _num_total_tasks(-1),
      activated(true) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this]() {
            while (activated) {
                if (_runnable == nullptr) {
                    continue;
                }
                int current = 0;
                while ((current = num_finished.fetch_add(1)) <
                       _num_total_tasks) {
                    _runnable->runTask(current, _num_total_tasks);
                }
            }
        });
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    activated = false;
    for (auto& thread : threads) {
        thread.join();
    }
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable,
                                               int num_total_tasks) {

    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    _runnable = runnable;
    _num_total_tasks = num_total_tasks;
    num_finished.store(0);

    while (num_finished < num_total_tasks) {}
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(
    IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(
    int num_threads)
    : ITaskSystem(num_threads),
      mu(std::mutex{}),
      cv(std::condition_variable{}),
      num_finished(std::atomic_int{0}),

      num_threads(num_threads),
      threads(std::vector<std::thread>{}),

      _runnable(nullptr),
      _num_total_tasks(-1),

      has_work(false),
      work_finished(false),
      activated(true) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this]() {
            while (activated) {
                {
                    std::unique_lock<std::mutex> lck{mu};
                    if (!has_work || activated) {
                        cv.wait(lck, [this] { return has_work || !activated; });
                    }
                    if (!activated) {
                        break;
                    }
                }
                int current = 0;
                while ((current = num_finished.fetch_add(1)) <
                       _num_total_tasks) {
                    std::cout << "running task " << current << " total " << _num_total_tasks << '\n';
                    _runnable->runTask(current, _num_total_tasks);
                }

                {
                    std::scoped_lock<std::mutex> lck{mu};
                    std::cout << "finished for " << num_finished.load() << '\n';
                    has_work = false;
                }

                // {
                //     std::unique_lock<std::mutex> lck{mu};
                //     work_finished = true;
                // }
                // cv.notify_all();
            }
        });
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    {
        std::scoped_lock<std::mutex> lck{mu};
        activated = false;
    }
    cv.notify_all();
    for (auto& thread : threads) {
        thread.join();
    }
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable,
                                               int num_total_tasks) {

    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    {
        std::scoped_lock<std::mutex> lck{mu};
        _runnable = runnable;
        _num_total_tasks = num_total_tasks;
        has_work = true;
        work_finished = false;
    }
    cv.notify_all();

    while (num_finished.load() < num_total_tasks) {
        std::this_thread::yield();
    }

    {
        std::scoped_lock<std::mutex> lck{mu};
        has_work = false;
    }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(
    IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {

    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}
