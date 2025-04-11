
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "AlertTask.h"

#include "TaskMaster.h"


TaskMaster::TaskMaster(Provider* p)
{
    provider = p;
}

TaskMaster::~TaskMaster()
{
}

Task* TaskMaster::launch(TaskMaster::Type type)
{
    // here is where you would look for existing instances of this task
    // and whether they allow concurrency

    Task* task = nullptr;
    switch (type) {

        case Alert: task = new AlertTask(provider); break;
            
        default: break;
    }
    
    if (task == nullptr) {
        Trace(1, "TaskMaster: Unable to launch task %d", type);
    }
    else {
        tasks.add(task);

        task->launch();

        // tasks don't ordinarilly complete right away but sometimes they do
        if (task->isFinished())
          tasks.removeObject(task, true);
    }

    return task;
}

void TaskMaster::finish(Task* t)
{
    tasks.removeObject(t, true);
}

void TaskMaster::cancel(Task* t)
{
    finish(t);
}


void TaskMaster::advance()
{
    // here is where you would walk the task list and remove the ones
    // that have finished, or that have timed out
}


    
