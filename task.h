#pragma once

/*
 * Abstract base class for thread pool's task
 */
class Task {

public:

    Task(){}

    virtual ~Task(){}

    virtual bool process() = 0;
};


