/**
 * Temporary model mapping functions.
 */

#include "../../util/Trace.h"

#include "../../model/UIEventType.h"
#include "../../model/ModeDefinition.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Setup.h"
#include "../../model/Preset.h"

#include "Event.h"
#include "Function.h"
#include "Parameter.h"
#include "Mode.h"
#include "Mobius.h"

#include "Mapper.h"

//////////////////////////////////////////////////////////////////////.
//
// Constant Objects
//
//////////////////////////////////////////////////////////////////////.

/**
 * Used by EventManager::getEventSummary to build the MobiusState.
 * 
 * Core EventType objects are strewn about all over the Function implementations
 * and have behavior methods with hidden core logic.
 * The UI needs to display these and have some basic information about them.
 * UIEventType is used only in MobiusState
 *
 * Ordinal mapping isn't possible because core events aren't maintained
 * in a static array and assigned ordinals.  I'll start by searching by
 * name which sucks, but we don't ususally have too many events at one time.
 *
 * This looks like a good candidate for the core subclassing the UIEventType
 * to add it's extra behavior.  Could also use a visitor pattern to avoid
 * the name search.
 */
UIEventType* MapEventType(EventType* src)
{
    UIEventType* uit = nullptr;
    if (src != nullptr) {
        uit = UIEventType::find(src->name);
        if (uit == nullptr) {
            Trace(1, "Mapper::MapEventType Unable to map type %s", src->name);
        }
    }
    return uit;
}

/**
 * This is used by Loop to map a core MobiusMode into a UI ModeDefinition.
 * Used only to build the MobiusState.
 *
 * MobiusMode has internal methods like invoke() so the model can't be shared.
 * They are maintained in an array so we could use ordinal mapping with some work.
 * They are simple enough that subclassing may be possible.
 */
class ModeDefinition* MapMode(class MobiusMode* mode)
{
    return (mode != nullptr) ? ModeDefinition::find(mode->name) : nullptr;
}

//////////////////////////////////////////////////////////////////////
//
// Files
//
// Used mostly by Project
// There were lots of uses of Audio->write in debugging code that
// was commented out.
//
// Move all of this to MobiusContainer which will also want control
// over the full path so we don't have absolute paths or assumptions
// about current working directory in the core.
//
//////////////////////////////////////////////////////////////////////

/**
 * Write the contents of an Audio to a file
 */
void WriteAudio(class Audio* a, const char* path)
{
    (void)a;
    (void)path;
    Trace(1, "Mapper: WriteAudio not implemented\n");
}

/**
 * Write a file with the given content.
 * Used by Project, I think to store the Project XML.
 */
// had to change on Mac because util/FileUtil also has a WriteFile
// sort this shit out
void WriteFileStub(const char* path, const char* content)
{
    (void)path;
    (void)content;
    Trace(1, "Mapper: WriteFileStub not implemented\n");
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
