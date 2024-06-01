
#include <JuceHeader.h>

#include <string>
#include <sstream>

#include "../../KeyTracker.h"
#include "../../Binderator.h"
#include "../../util/Trace.h"
#include "../../model/Binding.h"
#include "../common/Form.h"

#include "ConfigEditor.h"
#include "BindingPanel.h"
#include "KeyboardPanel.h"

KeyboardPanel::KeyboardPanel(ConfigEditor* argEditor) :
    BindingPanel(argEditor, "Keyboard Bindings", false)
{
    setName("KeyboardPanel");

    // now that BindingPanel is fully constructed
    // initialize the form so it can call down to our virtuals
    initForm();
}

KeyboardPanel::~KeyboardPanel()
{
    // make sure this doesn't linger
    KeyTracker::Instance.removeExclusiveListener(this);
}

/**
 * Called by ConfigEditor when we're about to be made visible.
 * Since we're not using the usual Juce component dispatching
 * for keyboard events have to add/remove our listener to the
 * global key tracker.  Don't really like this but there aren't
 * many places that need to mess with keyboard tracking and this
 * makes it easier than dealing with focus.
 */
void KeyboardPanel::showing()
{
    // use the newer "exclusive" listener to prevent Binderator
    // from going crazy while we capture key events
    //KeyTracker::addListener(this);
    KeyTracker::Instance.setExclusiveListener(this);
}

/**
 * Called by ConfigEditor when we're about to be made invisible.
 */
void KeyboardPanel::hiding()
{
    // KeyTracker::removeListener(this);
    KeyTracker::Instance.removeExclusiveListener(this);
}

/**
 * Called by BindingPanel as it iterates over all the bindings
 * stored in a BindingSet.  Return true if this is for keys.
 */
bool KeyboardPanel::isRelevant(Binding* b)
{
    return (b->trigger == TriggerKey);
}

/**
 * Return the string to show in the trigger column for a binding.
 * The Binding has a key code but we want to show a nice symbolic name.
 */
juce::String KeyboardPanel::renderSubclassTrigger(Binding* b)
{
    // unpack our compressed code/modifiers value
    int code;
    int modifiers;
    Binderator::unpackKeyQualifier(b->triggerValue, &code, &modifiers);
    return KeyTracker::getKeyText(code, modifiers);
}

/**
 * Overload of a BindingPanel virtual to insert our fields in between
 * scope and arguments.  Messy control flow and has constructor issues
 * with initForm.  Would be cleaner to give Form a way to insert into
 * existing Forms.
 */
void KeyboardPanel::addSubclassFields()
{
    key = new Field("Key", Field::Type::String);
    // needs to be wide enough to show the full text representation
    // including qualifiers
    key->setWidthUnits(20);
    form.add(key);

    capture = new Field("Capture", Field::Type::Boolean);
    form.add(capture);
}

/**
 * Refresh the key field to show the selected binding
 * Uses the same rendering as the table cell
 */
void KeyboardPanel::refreshSubclassFields(class Binding* b)
{
    key->setValue(renderTriggerCell(b));
}

/**
 * Capture current editing fields into the Binding.
 * Can be called with an empty [New] binding so must
 * initialize everything so it won't be filtered later
 * in XML rendering.
 */
void KeyboardPanel::captureSubclassFields(class Binding* b)
{
    b->trigger = TriggerKey;

    // undo the text transformation that was captured or typed in
    int code = 0;
    int modifiers = 0;
    juce::var value = key->getValue();
    KeyTracker::parseKeyText(value.toString(), &code, &modifiers);

    int myCode = Binderator::getKeyQualifier(code, modifiers);

    if (capture->getBoolValue() && capturedCode > 0) {
        // we're supposed to have saved the capture here
        b->triggerValue = capturedCode;

        // test to see if there are any conditions where the text transform
        // doesn't end up  with the same thing
        if (capturedCode != myCode)
          Trace(1, "KeyboardPanel: Key encoding anomoly %d %d\n",
                capturedCode, myCode);
    }
    else {
        // didn't have a capture, trust the text parse
        b->triggerValue = myCode;
    }
}

void KeyboardPanel::resetSubclassFields()
{
    key->setValue(juce::var());
}

// Hmm, we've got two ways to capture keyboard events.
// If we have focus then we get keyPressed from the juce::KeyListener.
// if we don't, then Supervisor usually has focus, passes the KeyPress
// through KeyTracker and KeyTracker::Listener calls keyTrackerDown.
// Hate this, if we don't define keyPressed will this still fall back
// to KeyTracker?
//
// Either way, KeyTracker also has Binderator as a listener, so it's going
// to be processing key actions while we're pondering about them.
//
// Actually, I don't think we ever received keyPressed directly, it always
// went through KeyTracker?  But I could have sworn I saw this once.

bool KeyboardPanel::keyPressed(const juce::KeyPress& keypress, juce::Component* originator)
{
    (void)originator;
    Trace(1, "KeyboardPanel::keyPressed  Sure wasn't expecting THAT to happen\n");
    
    if (capture->getBoolValue()) {
        key->setValue(juce::var(keypress.getTextDescription()));

        // format the Binderator "qualifier" for this key and save it
        // for captureSubclassFields
        // todo: once this is set, we'll always use it rather than the
        // text description, to ensure there isn't anything wonky with the text
        // conversion.  They're supposed to be the same though.
        capturedCode = Binderator::getKeyQualifier(keypress);
    }
        
    return false;
}

bool KeyboardPanel::keyStateChanged(bool isKeyDown, juce::Component* originator)
{
    (void)isKeyDown;
    (void)originator;
    return false;
}

void KeyboardPanel::keyTrackerDown(int code, int modifiers)
{
    if (capture->getBoolValue()) {
        key->setValue(juce::var(KeyTracker::getKeyText(code, modifiers)));
        capturedCode = Binderator::getKeyQualifier(code, modifiers);
    }
}

void KeyboardPanel::keyTrackerUp(int code, int modifiers)
{
    (void)code;
    (void)modifiers;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

