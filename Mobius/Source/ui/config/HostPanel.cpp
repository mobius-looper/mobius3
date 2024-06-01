/**
 * For plugin parameters we don't need to allow binding to all possible targets.
 * Need to find a way to reduce these.
 */

#include <JuceHeader.h>

#include <string>
#include <sstream>

#include "../../util/Trace.h"
#include "../../model/UIConfig.h"
#include "../../model/Binding.h"

#include "ConfigEditor.h"
#include "BindingPanel.h"
#include "HostPanel.h"

HostPanel::HostPanel(ConfigEditor* argEditor) :
    BindingPanel(argEditor, "Host Parameters", false)
{
    setName("HostPanel");

    // we don't need a trigger column
    bindings.removeTrigger();
    
    // now that BindingPanel is fully constructed
    // initialize the form so it can call down to our virtuals
    initForm();
}

HostPanel::~HostPanel()
{
}

/**
 * Called by BindingPanel as it iterates over all the bindings
 * stored in a BindingSet.  Return true if this is for host parameters.
 */
bool HostPanel::isRelevant(Binding* b)
{
    return (b->trigger == TriggerHost);
}

/**
 * Return the string to show in the trigger column for a binding.
 * The trigger column should be suppressed for buttons so we won't get here
 */
juce::String HostPanel::renderSubclassTrigger(Binding* b)
{
    (void)b;
    return juce::String();
}

/**
 * Add fields to the BindingPanel form, we have none.
 */
void HostPanel::addSubclassFields()
{
}

/**
 * Refresh local fields to reflect the selected binding.
 */
void HostPanel::refreshSubclassFields(Binding* b)
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
void HostPanel::captureSubclassFields(class Binding* b)
{
    b->trigger = TriggerHost;
}

void HostPanel::resetSubclassFields()
{
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
