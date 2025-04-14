
#include "AlertTask.h"

AlertTask::AlertTask()
{
    type = Task::Alert;
}

void AlertTask::run()
{
    finished = true;
}

void AlertTask::cancel()
{
    finished = true;
}
