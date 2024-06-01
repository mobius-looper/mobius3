/**
 * Temporary model mapping functions.
 */

#include "../../util/Trace.h"

#include "../../model/UIEventType.h"
#include "../../model/FunctionDefinition.h"
#include "../../model/UIParameter.h"
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
    return (src != nullptr) ? UIEventType::find(src->name) : nullptr;
}

/**
 * Used by EventManager::getEventSummary to build the MobiusState
 *
 * Core Event objects can reference a Function and we want to display
 * The function name associated with the event.
 *
 * This one can use ordinals when we get the mapping vector built out.
 */
class FunctionDefinition* MapFunction(class Function* src)
{
    return (src != nullptr) ? FunctionDefinition::find(src->name) : nullptr;
}

/**
 * Used by Track::resetParameteters to selectively reset parameters
 * stored in the Track-specific Setup copy after the Reset function.
 * This is done by calling this Setup method:
 *
 * if (global || setup->isResetable(MapParameter(InputLevelParameter))) {
 *
 * The Setup model was changed to use UIParameter for isResetable.
 * It maintains a StringList of the parameter names that should be sensitive
 * to reset and just looks for the name in that list.  This list is user
 * specified and can be different in each Setup so it can't go on the Parameter object.
 *
 * Could use ordinals here, but it is only done on Reset and the list is not typically
 * long.  Still, use ordinal mapping when the arrays are ready.
 *
 * This was only used for a few thigns related to Setup::isResetRetains,
 * don't need it now.
 */
class UIParameter* MapParameter(class Parameter* src)
{
    return (src != nullptr) ? UIParameter::find(src->name) : nullptr;
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
