
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "MslContext.h"
#include "MslExternal.h"

#include "ScriptExternals.h"

/**
 * Struct defining the association of a external name and the numeric id
 * Order is not significant
 */
ScriptExternalDefinition ScriptExternalDefinitions[] = {

    {"MidiOut", ExtMidiOut},

    {nullptr, ExtNone}

};

ScriptExternalId ScriptExternals::find(juce::String name)
{
    ScriptExternalId id = ExtNone;

    for (int i = 0 ; ScriptExternalDefinitions[i].name != nullptr ; i++) {
        if (strcmp(ScriptExternalDefinitions[i].name, name.toUTF8()) == 0) {
            id = ScriptExternalDefinitions[i].id;
            break;
        }
    }
    return id;
}

bool ScriptExternals::doAction(MslContext* c, MslAction* action)
{
    bool success = false;

    int intId = (int)action->external->object;

    if (intId >= 0 && intId < (int)ExtMax) {
        ScriptExternalId id = (ScriptExternalId)intId;

        switch (id) {
            case ExtMidiOut:
                success = MidiOut(c, action);
                break;

            default:
                Trace(1, "ScriptExternals: Unhandled external id %d", intId);
                break;
        }
    }
    else {
        Trace(1, "ScriptExternals: Invalid external id %d", intId);
    }

    return success;
}

//////////////////////////////////////////////////////////////////////
//
// Functions
//
//////////////////////////////////////////////////////////////////////

bool ScriptExternals::MidiOut(MslContext* c, MslAction* action)
{
    (void)c;
    (void)action;

    Trace(2, "ScriptExternals::MidiOut");
          
    return true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

