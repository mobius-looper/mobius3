
#include <JuceHeader.h>

#include "../../Supervisor.h"
#include "../../KeyTracker.h"
#include "../../Binderator.h"
#include "../../util/Trace.h"
#include "../../model/Binding.h"

#include "KeyboardEditor.h"

KeyboardEditor::KeyboardEditor(Supervisor* s) : OldBindingEditor(s)
{
    setName("KeyboardEditor");
    initForm();
}

KeyboardEditor::~KeyboardEditor()
{
    // make sure this doesn't linger
    supervisor->getKeyTracker()->removeExclusiveListener(this);
}

/**
 * Called by ConfigEditor when we're about to be made visible.
 * Since we're not using the usual Juce component dispatching
 * for keyboard events have to add/remove our listener to the
 * global key tracker.  Don't really like this but there aren't
 * many places that need to mess with keyboard tracking and this
 * makes it easier than dealing with focus.
 */
void KeyboardEditor::showing()
{
    // use the newer "exclusive" listener to prevent Binderator
    // from going crazy while we capture key events
    //KeyTracker::addListener(this);
    supervisor->getKeyTracker()->setExclusiveListener(this);
}

/**
 * Called by ConfigEditor when we're about to be made invisible.
 */
void KeyboardEditor::hiding()
{
    // KeyTracker::removeListener(this);
    supervisor->getKeyTracker()->removeExclusiveListener(this);
}

/**
 * Called by BindingEditor as it iterates over all the bindings
 * stored in a BindingSet.  Return true if this is for keys.
 */
bool KeyboardEditor::isRelevant(Binding* b)
{
    return (b->trigger == Binding::TriggerKey);
}

/**
 * Return the string to show in the trigger column for a binding.
 * The Binding has a key code but we want to show a nice symbolic name.
 */
juce::String KeyboardEditor::renderSubclassTrigger(Binding* b)
{
    // unpack our compressed code/modifiers value
    int code;
    int modifiers;
    Binderator::unpackKeyQualifier(b->triggerValue, &code, &modifiers);
    return KeyTracker::getKeyText(code, modifiers);
}

/**
 * Overload of a BindingEditor virtual to insert our fields in between
 * scope and arguments.  Messy control flow and has constructor issues
 * with initForm.  Would be cleaner to give Form a way to insert into
 * existing Forms.
 */
void KeyboardEditor::addSubclassFields()
{
#if 0    
    key = new Field("Key", Field::Type::String);
    // needs to be wide enough to show the full text representation
    // including qualifiers
    key->setWidthUnits(20);
    key->addListener(this);
    form.add(key);
#endif
    
    // note that the subclass does not listen, but BindingEditor does
    key.setListener(this);
    form.add(&key);
    // stick a release selector next to it
    addRelease();
}

/**
 * Refresh the key field to show the selected binding
 * Uses the same rendering as the table cell
 */
void KeyboardEditor::refreshSubclassFields(class Binding* b)
{
    key.setValue(renderTriggerCell(b));
}

/**
 * Capture current editing fields into the Binding.
 * Can be called with an empty [New] binding so must
 * initialize everything so it won't be filtered later
 * in XML rendering.
 */
void KeyboardEditor::captureSubclassFields(class Binding* b)
{
    b->trigger = Binding::TriggerKey;

    // undo the text transformation that was captured or typed in
    int code = 0;
    int modifiers = 0;
    juce::String value = key.getValue();
    KeyTracker::parseKeyText(value, &code, &modifiers);

    int myCode = Binderator::getKeyQualifier(code, modifiers);

    if (capture.getValue() && capturedCode > 0) {
        // we're supposed to have saved the capture here
        b->triggerValue = capturedCode;

        // test to see if there are any conditions where the text transform
        // doesn't end up  with the same thing
        if (capturedCode != myCode)
          Trace(1, "KeyboardEditor: Key encoding anomoly %d %d\n",
                capturedCode, myCode);
    }
    else {
        // didn't have a capture, trust the text parse
        b->triggerValue = myCode;
    }
}

void KeyboardEditor::resetSubclassFields()
{
    key.setValue("");
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

bool KeyboardEditor::keyPressed(const juce::KeyPress& keypress, juce::Component* originator)
{
    (void)originator;
    Trace(1, "KeyboardEditor::keyPressed  Sure wasn't expecting THAT to happen\n");

    juce::String keytext = keypress.getTextDescription();

    if (isCapturing()) {
        key.setValue(keytext);

        // format the Binderator "qualifier" for this key and save it
        // for captureSubclassFields
        // todo: once this is set, we'll always use it rather than the
        // text description, to ensure there isn't anything wonky with the text
        // conversion.  They're supposed to be the same though.
        capturedCode = Binderator::getKeyQualifier(keypress);
    }

    showCapture(keytext);
        
    return false;
}

bool KeyboardEditor::keyStateChanged(bool isKeyDown, juce::Component* originator)
{
    (void)isKeyDown;
    (void)originator;
    return false;
}

void KeyboardEditor::keyTrackerDown(int code, int modifiers)
{
    juce::String keytext = KeyTracker::getKeyText(code, modifiers);
    if (isCapturing()) {
        key.setValue(keytext);
        capturedCode = Binderator::getKeyQualifier(code, modifiers);
    }

    showCapture(keytext);
}

void KeyboardEditor::keyTrackerUp(int code, int modifiers)
{
    (void)code;
    (void)modifiers;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

