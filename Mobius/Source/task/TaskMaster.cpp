
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "AlertTask.h"
#include "ProjectExportTask.h"
#include "ProjectImportTask.h"

#include "Task.h"
#include "TaskMaster.h"


TaskMaster::TaskMaster(Provider* p)
{
    provider = p;
}

TaskMaster::~TaskMaster()
{
}

int TaskMaster::launch(Task::Type type)
{
    int id = 0;
    
    // here is where you would look for existing instances of this task
    // and whether they allow concurrency

    Task* task = nullptr; 
    switch (type) {

        case Task::Alert: task = new AlertTask(); break;
        case Task::ProjectExport: task = new ProjectExportTask(); break;
        case Task::ProjectImport: task = new ProjectImportTask(); break;
            
        default: break;
    }
    
    if (task == nullptr) {
        Trace(1, "TaskMaster: Unable to launch task %d", type);
    }
    else if (task->getType() != type) {
        Trace(1, "TaskMaster: Mismatched track type for %s", task->getTypeName());
        delete task;
    }
    else {
        id = launch(task);
    }
    return id;
}

int TaskMaster::launch(Task* task)
{
    int id = 0;
    
    if (task->isConcurrent() || (find(task->getType()) == nullptr)) {
        
        id = ++TaskIds;
        task->setId(id);
    
        // task must have been created with a type
        tasks.add(task);
        task->launch(provider);
        if (task->isFinished()) {
            Trace(2, "TaskMaster: Task ran to completion without suspending %s", task->getTypeName());
            tasks.removeObject(task, true);
        }
    }
    else {
        Trace(1, "TaskMaster: Attempt to launch concurrent task %s", task->getTypeName());
        delete task;
    }
    
    return id;
}

void TaskMaster::finish(int id)
{
    Task* t = find(id);
    if (t == nullptr)
      Trace(2, "TaskMaster::finish No task with id %d", id);
    else {
        finish(t);
    }
}

void TaskMaster::finish(Task* task)
{
    Trace(2, "TaskMaster: Finishing task %s", task->getTypeName());
    tasks.removeObject(task, true);
}

void TaskMaster::cancel(int id)
{
    Task* t = find(id);
    if (t != nullptr) {
        Trace(2, "TaskMaster: Canceling task %s", t->getTypeName());
        finish(t);
    }
}

juce::OwnedArray<Task>& TaskMaster::getTasks()
{
    return tasks;
}

void TaskMaster::advance()
{
    int index = 0;
    while (index < tasks.size()) {
        Task* t = tasks[index];
        if (t->isFinished()) {
            Trace(2, "TaskMaster: Reclaiming finished task %s", t->getTypeName());
            finish(t);
        }
        else {
            index ++;
        }
    }
}
    
Task* TaskMaster::find(Task::Type type)
{
    Task* found = nullptr;
    for (auto task : tasks) {
        if (task->getType() == type) {
            found = task;
            break;
        }
    }
    return found;
}

Task* TaskMaster::find(int id)
{
    Task* found = nullptr;
    for (auto task : tasks) {
        if (task->getId() == id) {
            found = task;
            break;
        }
    }
    return found;
}

            
