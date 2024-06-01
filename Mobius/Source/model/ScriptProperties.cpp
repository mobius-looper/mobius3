
#include "../util/Trace.h"

#include "ScriptProperties.h"

// broke these out trying to debug a memory leak, if nothing
// else ends up in this file,  can just inline them in ScriptProperties.h
// like we do for SampleProperties

ScriptProperties::ScriptProperties()
{
    //Trace(2, "ScriptProperties: allocate\n");
}

ScriptProperties::~ScriptProperties()
{
    //Trace(2, "ScriptProperties: free\n");
}
