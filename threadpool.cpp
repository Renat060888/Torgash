
// project
#include "threadpool.h"

using namespace std;

ThreadPool::ThreadPool( const int _threads ) :
    m_terminate(false),
    m_stopped(false){

    for(int i = 0; i < _threads; i++){

        m_pool.emplace_back( thread(&ThreadPool::Worker, this, i) );
    }
}

void ThreadPool::enqueue( Task *_task ){

	{
        std::unique_lock<std::mutex> lock(m_tasksMutex);

        m_tasks.push( _task );
	}

    m_condition.notify_one();
}

void ThreadPool::Worker( int16_t /*_threadID*/ ){

    Task * task;

    while(true){

		{
            std::unique_lock<std::mutex> lock(m_tasksMutex);

			// Wait until queue is not empty or termination signal is sent.
            m_condition.wait( lock, [this]{ return !m_tasks.empty() || m_terminate; } );

            if( m_terminate && m_tasks.empty() ){

				return;
			}

            task = m_tasks.front();
            m_tasks.pop();
		}

        task->process();
	}
}

void ThreadPool::shutdown(){

	{
        std::unique_lock<std::mutex> lock(m_tasksMutex);

        m_terminate = true;
	}

    m_condition.notify_all();

    for( std::thread & thread : m_pool ){

		thread.join();
	}

    m_pool.empty(); // TODO ?

    m_stopped = true;
}

ThreadPool::~ThreadPool(){

    if( ! m_stopped ){

        shutdown();
	}
}

