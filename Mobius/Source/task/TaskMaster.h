/**
 * Manages the launching, monitoring, and cleanup of tasks.
 */

#include <JuceHeader.h>


class TaskMaster
{
  public:

    typedef enum {
        None,
        Alert
    } Type;
    

    TaskMaster(class Provider* p);
    ~TaskMaster();

    class Task* launch(Type t);
    void finish(class Task* t);
    void cancel(class Task* t);

    void advance();
    
  private:

    class Provider* provider = nullptr;
    juce::OwnedArray<class Task> tasks;

};
