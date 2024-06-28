/**
 * For plugin parameters we don't need to allow binding to all possible targets.
 * Need to find a way to reduce these.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/UIConfig.h"
#include "../../model/Binding.h"

#include "HostEditor.h"

HostEditor::HostEditor()
{
    setName("HostEditor");

    // we don't need a trigger column
    bindings.removeTrigger();
    
    // now that BindingPanel is fully constructed
    // initialize the form so it can call down to our virtuals
    initForm();
}

HostEditor::~HostEditor()
{
}

/**
 * Called by BindingPanel as it iterates over all the bindings
 * stored in a BindingSet.  Return true if this is for host parameters.
 */
bool HostEditor::isRelevant(Binding* b)
{
    return (b->trigger == TriggerHost);
}

/**
 * Return the string to show in the trigger column for a binding.
 * The trigger column should be suppressed for buttons so we won't get here
 */
juce::String HostEditor::renderSubclassTrigger(Binding* b)
{
    (void)b;
    return juce::String();
}

/**
 * Add fields to the BindingPanel form, we have none.
 */
void HostEditor::addSubclassFields()
{
}

/**
 * Refresh local fields to reflect the selected binding.
 */
void HostEditor::refreshSubclassFields(Binding* b)
{
    (void)b;
}

/**
 * Capture current editing fields into the Binding.
 * Can be called with an empty [New] binding so must
 * initialize everything so it won't be filtered later
 * in XML rendering.
 *
 * Host bindings do not have a value, only an operation.
 */
void HostEditor::captureSubclassFields(class Binding* b)
{
    b->trigger = TriggerHost;
}

void HostEditor::resetSubclassFields()
{
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
