
#include <JuceHeader.h>

#include "Task.h"

class AlertTask : public Task
{
  public:
    
    AlertTask(class Provider* p);

    void run() override;
};


