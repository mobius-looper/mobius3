
#include <JuceHeader.h>

#include "MslError.h"
#include "MslScript.h"
#include "ScriptFile.h"

ScriptFile::ScriptFile()
{
}

ScriptFile::~ScriptFile()
{
    delete script;
}

