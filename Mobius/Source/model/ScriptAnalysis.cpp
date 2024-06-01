
#include "../util/List.h"

#include "ScriptAnalysis.h"

ScriptAnalysis::ScriptAnalysis()
{
    mNames = nullptr;
    mButtons = nullptr;
    mErrors = nullptr;
}

ScriptAnalysis::~ScriptAnalysis()
{
    delete mNames;
    delete mButtons;
    delete mErrors;
}

void ScriptAnalysis::addName(const char* name)
{
    if (mNames == nullptr)
      mNames = new StringList();
    mNames->add(name);
}

void ScriptAnalysis::addError(const char* error)
{
    if (mErrors == nullptr)
      mErrors = new StringList();
    mErrors->add(error);
}

void ScriptAnalysis::addButton(const char* button)
{
    if (mButtons == nullptr)
      mButtons = new StringList();
    mButtons->add(button);
}

