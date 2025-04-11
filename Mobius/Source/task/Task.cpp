#include <JuceHeader.h>

#include "../util/Trace.h"

#include "Task.h"

Task::Task(Provider* p)
{
    provider = p;
}

Task::~Task()
{
}

void Task::launch()
{
    finished = false;
    run();
}

bool Task::isFinished()
{
    return finished;
}
