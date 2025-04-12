#include <JuceHeader.h>

#include "../util/Trace.h"

#include "Task.h"

Task::Task()
{
}

Task::~Task()
{
}

Task::Type Task::getType()
{
    return type;
}

void Task::setId(int i)
{
    id = i;
}

int Task::getId()
{
    return id;
}

const char* Task::getTypeName()
{
    const char* name = "???";
    switch (type) {
        case None: name = "None"; break;
        case Alert: name = "Alert"; break;
        case ProjectExport: name = "ProjectExport"; break;
        case ProjectImport: name = "ProjectImport"; break;
    }
    return name;
}

void Task::launch(Provider* p)
{
    provider = p;
    finished = false;
    run();
}

bool Task::isFinished()
{
    return finished;
}

void Task::addError(juce::String e)
{
    errors.add(e);
}

bool Task::hasErrors()
{
    return (errors.size() > 0);
}

void Task::addWarning(juce::String w)
{
    warnings.add(w);
}
