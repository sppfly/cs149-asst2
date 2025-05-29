
#include "tasksys.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <mutex>
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
      num_started(std::atomic_int{0}),

      _runnable(nullptr),
      _num_total_tasks(-1),

      mu(std::mutex{}),
      shutdown(false) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this]() {
            while (true) {
                {
                    std::scoped_lock<std::mutex> lck{mu};
                    if (shutdown) {
                        break;
                    }
                    if (_runnable == nullptr) {
                        continue;
                    }
                }

                if (num_finished > _num_total_tasks) {
                    continue;
                }

                int current = 0;
                while ((current = num_started.fetch_add(1)) <
                       _num_total_tasks) {
                    _runnable->runTask(current, _num_total_tasks);
                    num_finished.fetch_add(1);
                }
            }
        });
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    {
        std::scoped_lock<std::mutex> lck{mu};
        shutdown = true;
    }
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

    {
        std::scoped_lock<std::mutex> lck{mu};
        _runnable = runnable;
        _num_total_tasks = num_total_tasks;
        num_finished.store(0);
        num_started.store(0);
    }

    while (num_finished < num_total_tasks) {}

    // it is very tricky to determine whether all jobs have been finished
    {
        std::scoped_lock<std::mutex> lck{mu};
        _runnable = nullptr;
        _num_total_tasks = -1;
    }
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
      start_cv(std::condition_variable{}),
      finish_cv(std::condition_variable{}),

      num_threads(num_threads),
      threads(std::vector<std::thread>{}),

      _runnable(nullptr),
      _num_total_tasks(-1),

      has_work(false),
      shutdown(false) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    
    
   
   
    daemon = std::thread {[this]() {
        while (true) {
            {
                std::scoped_lock<std::mutex> lck {mu};
                if (shutdown) {
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }};
    
    
    
    
    
    

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this]() {
            while (true) {
                {
                    // std::unique_lock<std::mutex> lck{mu};
                    // start_cv.wait(lck, [this] { return shutdown; });
                    // if (shutdown) {
                    //     break;
                    // }
                }
                int current;
                // possible optimization here, use atomic instead of mutex
                IRunnable* runnable;
                int num_total_tasks;
                {
                    std::scoped_lock<std::mutex> lck {mu};
                    if (!ready_tasks.empty()) {
                        auto it = std::ranges::find_if(ready_tasks, [](Task task) {
                            return task.num_started < task.num_total_tasks;
                        });
                        if (it == ready_tasks.end()) {
                            // this is an extreme condition when all tasks are running
                            continue;
                        }
                        Task& task = *it; 
                        current = task.num_started;
                        task.num_started++;
                        runnable = task.runnable;
                        num_total_tasks = task.num_total_tasks;
                    }
                }
                
                runnable->runTask(current, num_total_tasks);
                
                

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
        shutdown = true;
    }
    start_cv.notify_all();
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

    // {
    //     std::scoped_lock<std::mutex> lck{mu};
    //     _runnable = runnable;
    //     _num_total_tasks = num_total_tasks;
    //     num_finished = 0;
    //     work_finished = false;
    //     has_work = true;
    // }
    // start_cv.notify_all();

    // while (num_finished < _num_total_tasks) {
    //     // TODO: somehow if I all this sleep it will not work, WHY?
    //     // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // }

    // {
    //     std::scoped_lock<std::mutex> lck {mu};
    //     has_work = false;
    // }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(
    IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {

    //
    // TODO: CS149 students will implement this method in Part B.
    //
    std::scoped_lock<std::mutex> lck {mu};
    TaskID task_id = next_task_id;
    next_task_id += 1;
    // some optimizition possible here, we are copying memory  
    Task task = Task {.id=task_id, .runnable=runnable, .num_total_tasks=num_total_tasks};
    waiting_tasks.emplace_back(WaitTask{.waiting_for=deps, .task=task});
    return task_id;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}
