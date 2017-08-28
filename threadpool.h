#pragma once

// std
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <unistd.h>
// project
#include "task.h"

class ThreadPool{

public:

    ThreadPool( const int _threads );

    ~ThreadPool();

    void enqueue( Task * _task );

    void shutdown();

private:

    void Worker( int16_t _threadID );

    std::vector<std::thread>                    m_pool;

    std::queue< Task * >                        m_tasks;

    std::mutex                                  m_tasksMutex;

    std::condition_variable                     m_condition;

    bool                                        m_terminate;

    bool                                        m_stopped;
};

