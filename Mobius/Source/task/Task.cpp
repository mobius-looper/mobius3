#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../ui/common/YanDialog.h"

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
        case DialogTest: name = "DialogTest"; break;
        case Alert: name = "Alert"; break;
        case ProjectExport: name = "ProjectExport"; break;
        case SnapshotImport: name = "SnapshotImport"; break;
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

bool Task::hasMessages()
{
    return (messages.size() > 0 || errors.size() > 0 || warnings.size() > 0);
}

bool Task::hasErrors()
{
    return (errors.size() > 0);
}

void Task::clearMessages()
{
    messages.clear();
    warnings.clear();
    errors.clear();
}

void Task::transferMessages(YanDialog* d)
{
    for (auto m : messages)
      d->addMessage(m);
    for (auto w : warnings)
      d->addWarning(w);
    for (auto e : errors)
      d->addError(e);
}

void Task::addMessage(juce::String m)
{
    messages.add(m);
}

void Task::addError(juce::String e)
{
    errors.add(e);
}

void Task::addErrors(juce::StringArray& list)
{
    for (auto e : list)
      errors.add(e);
}

void Task::addWarning(juce::String w)
{
    warnings.add(w);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
