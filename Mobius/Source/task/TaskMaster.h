/**
 * Manages the launching, monitoring, and cleanup of tasks.
 */

#include <JuceHeader.h>

#include "Task.h"

class TaskMaster
{
  public:


    TaskMaster(class Provider* p);
    ~TaskMaster();

    int launch(Task::Type t);
    int launch(Task* task);
    
    juce::OwnedArray<class Task>& getTasks();
    
    void finish(int id);
    void cancel(int id);

    void advance();

    class Task* find(Task::Type t);
    class Task* find(int id);
    
  private:

    class Provider* provider = nullptr;
    juce::OwnedArray<class Task> tasks;
    int TaskIds = 0;
    
    void finish(class Task* task);

};
