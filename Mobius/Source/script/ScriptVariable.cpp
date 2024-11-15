
#include "ScriptVariable.h"

ScriptVariableDefinition ScriptVariableDefinitions[] = {

    {"trackNumber", VarTrackNumber},

    //
    // Terminator
    //
    
    {nullptr, VarNone}
};

ScriptVariableId ScriptVariableDefinition::find(const char* name)
{
    ScriptVariableId id = VarNone;
    for (int i = 0 ; ScriptVariableDefinitions[i].name != nullptr ; i++) {
        if (strcmp(name, ScriptVariableDefinitions[i].name) == 0) {
            id = ScriptVariableDefinitions[i].id;
            break;
        }
    }
    return id;
}

void ScriptVariableHandler::get(MslContext* c, ScriptVariableId id, MslValue* value)
{
    value->setNull();
    
    switch (id) {
        case VarTrackNumber:
            break;

        default:
            break;
    }
}
