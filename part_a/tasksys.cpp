#include "tasksys.h"
#include <atomic>
#include <chrono>
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
      num_finished(std::atomic_int {0}),
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
                while ((current = num_finished.fetch_add(1)) < _num_total_tasks) {
                    // std::osyncstream(std::cout) << "run task for " << current << '\n';
                    _runnable->runTask(current, _num_total_tasks);
                }
            }
            // std::osyncstream(std::cout) << "this thread comes to the end: "
                                        // << std::this_thread::get_id() << '\n';
        });
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    activated = false;
    // std::osyncstream(std::cout) << "destructor\n";
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


    while (num_finished < num_total_tasks) {
        // std::this_thread::yield();
        
    }

    // std::osyncstream(std::cout) << "finish run\n";
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
    num_threads(num_threads),
    threads(std::vector<std::thread> {}) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this]() {
            while (activated) {
                std::scoped_lock<std::mutex> lck {mu};

                // std::cout << "this is thread " << std::this_thread::get_id() << '\n';
            }
            // std::cout << "thread terminated\n";
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
    std::cout << "ask worker threads to end\n";
    activated = true;
    for (auto& thread: threads) {
        thread.join();
        std::cout << "thread" << thread.get_id() << " ends\n";
    }
    std::cout << "desturctor\n";
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable,
                                               int num_total_tasks) {

    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    _runnable = runnable;
    _num_total_tasks = num_total_tasks;

    work_finished = false;
    // {
    //     std::scoped_lock<std::mutex> lck {mu};
    //     has_work = true;
    // }
    // cv.notify_all();

    // {
    //     std::unique_lock<std::mutex> lck {};
        
    // }





    
    // for (int i = 0; i < num_total_tasks; i++) {
    //     runnable->runTask(i, num_total_tasks);
    // }
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
