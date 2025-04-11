
#include "AlertTask.h"

AlertTask::AlertTask(Provider* p) : Task(p)
{
}

void AlertTask::run()
{
    finished = true;
}

