/*
 * Helper class to monitor keyboard key transitions.
 * This must be registered as a KeyListener at an appropriate
 * point during application or plugin editor initialization.
 *
 * Juce has a somewhat understandable but annoying lack of sending
 * events for key up transitions.  You get notified with keyStateChanged
 * when anything goes up but you don't know what it was.  Most of the time
 * this doesn't matter but for few new Mobius functions that support sustainability
 * we want up transitions.
 *
 * To do that you have to monitor keyPress events and remember all the keys that
 * are currently down.  When you get keyStateChanged with isKeyDown=false, you have
 * to look in that down list and call isCurrentlyDown to see if it is still down
 * and if not, the keyStateChanged event represents an up transition of that
 * one and in theory any of the others that were formerly down if you're
 * mashing multiple keys.
 *
 * Modifier keys have not been used in the past but you can make it work for bindings.
 * You can't call isCurrentlyDown for modifier keys, but you can do this.
 *
 * On keyPressed
 *    save the key down with it's modifier keys
 *    send a custom key down event with the modifiers which may be ignored
 *
 * On keyStateChanged
 *    if the previous down key is no longer down send a custom key up event
 *    with the same modifiers it had when it went down
 *
 * Those original modifiers may not actually be down now, but it doesn't matter
 * in practice.  If you press ctrl-shift-A then while holding A release ctrl
 * when A goes up you don't look for a shift-A binding.  The binding characteristics
 * of A apply to the down transition, not the up.  Even if we could do that it would
 * be impossible to explain and use reliably.
 *
 * It should also be noted that Juce doesn't give you raw scan codes, it does
 * interpretation on those and provides normalized codes.  Some of the extended
 * keys liks F1, Arrows, etc. come in with bit 0x10000 set.  These are not the same
 * numbers that some other encoding schemes use.  It may be a standard but I couldn't
 * figure out what it was and spent way too much time trying to figure it out.  This
 * doesn't so much matter but if you want to use Juce getTextDescription and
 * createFromDescription to display and parse symbolic key names you have to preserve
 * bit 17 in the key code.  This is what we now store in Binding objects.
 *
 * When associating a key with an action to perform, I like to use a jump table,
 * and historically 8 bits has been enough and using 17 bits results in much wasted space.
 * For bindings, you can mask off the bottom 8 bits of a key code to use as a jump
 * table index with two exceptions:
 *
 *    0x2e is . and 0x1002e is "delete" from the command keypad
 *    0x2d is - and 0x1002d is "insert"
 *
 * I think it's reasonable to disallow mapping of those two as the command pad
 * is unreliable anyway what with PRTSC, SCRLK and PAUSE up there doing nothing good.
 *
 * Function keys and arrow keys are however used (by me anyway) and work reliably
 * after masking.
 *
 * The number pad is an absolute mess without scan codes.  It changes behavior
 * based on NumLock, some keys are the same as regular keys, and Juce strangely
 * generates double keyPressed events for some of them.  For example Number pad 7
 * with NumLock generates these two events:
 *
 *    0x10067 numpad 7
 *    0x1007a F11
 *
 * Without NumLock some keys generate the same codes as the arrow keys so they're
 * not that useful in bindings and have the same double key event problem.
 *
 * We could supress the duplicate event, but why bother.  
 * 
 * KEY REPEAT
 *
 * Another common complaint I see on the webz is that Juce passes through the OS
 * key repeat which is never what we want for bindings.  Those are suppressed since
 * they seem to happen surprisingly often during testing.  Or maybe it's just
 * when I'm angry.
 *
 * SHIFT KEY BINDINGS
 *
 * I have not historically allowed bindings to shifted ASCII key codes like * ( % etc.
 * If we pass through modifiers to the binding resolver this could be made to work
 * but a single jump table won't work since A and shift-A are still 0x41 in 8 bits.
 * Alternately, we could convert these to ASCII codes for the jump table would could
 * be nice if you want "p" to be Play and "P" to be Pause.  But you'd have to fit
 * in non-ASCII codes like function and arrow keys, and no one besides me uses key
 * bindings much anyway.  Fidling with ctrl-shift-alt while performing doesn't happen.
 *
 * Key codes are weird, see notes/keycodes.txt
 */


#include <JuceHeader.h>

#include "util/Trace.h"

#include "KeyTracker.h"

KeyTracker::KeyTracker()
{
}

KeyTracker::~KeyTracker()
{
}

void KeyTracker::addListener(Listener* l)
{
    if (!listeners.contains(l))
      listeners.add(l);
}

void KeyTracker::removeListener(Listener* l)
{
    listeners.removeAllInstancesOf(l);
}

/**
 * Special case for KeyboardPanel that wants to intercept key event
 * when editing the keyboard binding table, but while that happens
 * we want to avoid passing it to the regular listeners, most notably
 * Binderator which will start firing off actions to the engine.
 */
void KeyTracker::setExclusiveListener(Listener* l)
{
    if (exclusiveListener != nullptr)
      Trace(1, "KeyTracker: Overlapping exclusive listeners!\n");
    exclusiveListener = l;
}

/**
 * KeyboardPanel calls this on shutdown, do don't whine if we if already
 * removed itself like it should have.
 */
void KeyTracker::removeExclusiveListener(Listener* l)
{
    if (exclusiveListener != nullptr) {
        if (exclusiveListener != l)
          Trace(1, "KeyTracker: Someone stole the exclusive listener!\n");
        else
          exclusiveListener = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Juce Events
//
//////////////////////////////////////////////////////////////////////

/**
 * Return true to indiciate that the key has been consumed.
 * getKeyCode returns
 *  "This will either be one of the special constants defined in this class,
 *   or an 8-bit character code"
 *
 * Constants include things like spaceKey, backspaceKey, numberPad9, etc.
 *
 * todo: With the introduction of the Binderator "qualifier" encoding could
 * combine the key code with the modifier bits and just have one thing to save.
 * 
 */
bool KeyTracker::keyPressed(const juce::KeyPress& key, juce::Component* originator)
{
    (void)originator;
    // traceKeyPressed(key, originator);
    // could combine these into one array
    int code = key.getKeyCode();
    int mods = key.getModifiers().getRawFlags();

    // suppress key repeat
    // One annoyance here is that if you are in the debugger and hit a breakpoint
    // during the processing of keyTrackerDown, we won't receive an up event, I guess
    // because the debugger stole focus and ate it.  If you continue and press the key
    // again, KeyTracker thinks the second press is a repeat if the first one.
    // Not really  worth adding a timeout as this only happens under very specific conditions
    bool repeating = false;
    for (int i = 0 ; i < pressedCodes.size() ; i++) {
        if (pressedCodes[i] == code) {
            repeating = true;
            break;
        }
    }

    if (!repeating) {
        pressedCodes.add(code);
        pressedModifiers.add(mods);
        //traceTrackerDown(code, mods);

        if (exclusiveListener != nullptr) {
            exclusiveListener->keyTrackerDown(code, mods);
        }
        else {
            for (int i = 0 ; i < listeners.size() ; i++) {
                Listener* l = listeners[i];
                l->keyTrackerDown(code, mods);
            }
        }
    }
    
    return false;
}

bool KeyTracker::keyStateChanged(bool isKeyDown, juce::Component* originator)
{
    (void)originator;
    // traceKeyStateChanged(isKeyDown, originator);
    if (!isKeyDown) {
        int index = 0;
        while (index < pressedCodes.size()) {
            
            int code = pressedCodes[index];
            if (juce::KeyPress::isKeyCurrentlyDown(code)) {
                // on to the next one, yu hold so many keys?
                index++;
            }
            else {
                int mods = pressedModifiers[index];
                //traceTrackerUp(code, mods);

                if (exclusiveListener != nullptr) {
                    exclusiveListener->keyTrackerUp(code, mods);
                }
                else {
                    for (int i = 0 ; i < listeners.size() ; i++) {
                        Listener* l = listeners[i];
                        l->keyTrackerUp(code, mods);
                    }
                }
                
                // remove from the trackers
                // this would be better as a linked list to avoid shift
                // overhead, but we really shouldn't be dealing with many
                // concurrent pressed keys
                pressedCodes.remove(index);
                pressedModifiers.remove(index);
                // leave index where it is for the next one
            }
        }
    }
    
    return false;
}

//////////////////////////////////////////////////////////////////////
//
// Static Utilities
//
//////////////////////////////////////////////////////////////////////

/**
 * Utility to generate text descriptions for display.
 */
juce::String KeyTracker::getKeyText(int code, int modifiers)
{
    // third arg "may be null if the keypress is a non printing character"
    // wtf does that mean?
    juce::KeyPress kp (code, juce::ModifierKeys(modifiers), (juce::juce_wchar)0);
    return kp.getTextDescription();
}

int KeyTracker::parseKeyText(juce::String text)
{
    juce::KeyPress kp = juce::KeyPress::createFromDescription(text);
    return kp.getKeyCode();
}

void KeyTracker::parseKeyText(juce::String text, int* code, int* modifiers)
{
    juce::KeyPress kp = juce::KeyPress::createFromDescription(text);
    if (code != nullptr) *code = kp.getKeyCode();
    if (modifiers != nullptr) *modifiers = kp.getModifiers().getRawFlags();
}

//////////////////////////////////////////////////////////////////////
//
// Diagnostics
//
//////////////////////////////////////////////////////////////////////

void KeyTracker::traceKeyPressed(const juce::KeyPress& key, juce::Component* originator)
{
    (void)originator;
    // strip out the combo modifier enums
    int mods = key.getModifiers().getRawFlags();
    int basemods = ((long)mods & 0xFF);

    trace("%d %08x %08x %s %s\n", key.getKeyCode(), key.getKeyCode(), basemods,
          juce::String(key.getTextCharacter()).toUTF8(),
          key.getTextDescription().toUTF8());
}

void KeyTracker::traceKeyStateChanged(bool isKeyDown, juce::Component* originator)
{
    (void)originator;
    trace("keyStateChanged: %s\n", (isKeyDown) ? "down" : "up");
}

void KeyTracker::traceTrackerDown(int code, int modifiers)
{
    // can't create one of these without third arg
    // do you really have to calculatre this just to call getTextDescription?
    trace("KeyTracker down %s\n", getKeyText(code, modifiers).toUTF8());
}

void KeyTracker::traceTrackerUp(int code, int modifiers)
{
    trace("KeyTracker up %s\n", getKeyText(code, modifiers).toUTF8());
}

void KeyTracker::dumpCodes()
{
    for (int i = 0 ; i < 256 ; i++) {
        juce::KeyPress kp(i);

        trace("%d %08x %s %s\n", i, i, juce::String(kp.getTextCharacter()).toUTF8(),
              kp.getTextDescription().toUTF8());
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
