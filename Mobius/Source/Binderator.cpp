/**
 * There are several parts to this.
 * 
 * A Binderator maintains the mapping models between
 * external events and UIActions.
 *
 * ApplicationBinderator contains a Binderator and adds
 * listeners for keyboard and MIDI events when Mobius runs
 * as a standalone application.  It is managed at the UI
 * level by Supervisor.
 *
 * PluginBinderator builds a Binderator that handles MIDI
 * events only and is managed by MobiusKernel since MIDI
 * events for plugins come in on the audio thread.
 *
 * PluginEditorBinderator builds a Bindarator that handles
 * keyboard events only and is managed by Supervisor when
 * it is runing as plugin, and the plugin editor window is opened.
 * 
 * Key Code Notes
 *
 * Mapping keyboard keys with a jump table results in a very large
 * "namespace" when you combine scan codes with all of the modifier key
 * combinations.  I don't want megabytes of storage for a mostly sparse array,
 * yet would like it as fast as possible, or at least fast enough not to keep
 * me up at night.  This is somewhat more complex than the old code because
 * Juce doesn't pass raw "scan codes", it does some amount of interpretation
 * on them.  KeyPress codes are mostly standard ASCII/unicode but they include
 * shifted and unshifted characters.  ModifierKeys is a bit mask that includes
 * ctrl, alt, shift, command, and mouse buttons.
 *
 * So you don't test for the 'A' scan code with Shift down, you just get the
 * uppercase A keyCode.  This is better than scan codes anyway since I assume
 * they are machine independent.  I spent way too much time twiddling bits to
 * try and make the most optimal jump structure incorporating KeyPress codes
 * and ModifierBits and settled on this which works well enough.
 *
 * When a KeyPress is received from a KeyListener:
 *
 *    - get the keyCode and mask off the bottom byte for a 256 slot jump table
 *    - each element in the jump table is a pointer to a list of TableEntry structures
 *    - TableEntry includes the full juce key code and modifier keys and a pointer
 *      to the application target object
 *
 * This is effectively a HashMap where the hash key is the bottom byte of the keyCode.
 * For each key you then have to do a linear search looking for full code/modifier
 * combinations but in practice there will almost never be more than one, except for
 * me because I have the emacs taint.  In practice few if any users use key
 * bindings so I'm calling it a day.
 *
 * MIDI Notes
 *
 * MIDI has a more predictable and constrained message structure.
 * See notes/midicodes.txt for details.
 *
 * Three jump tables are maintained for each of the three major message types:
 * notes, program changes, continuous controllers.
 *
 * The first data byte (note number, program number, cc, number) is used as
 * the index into this table.
 *
 * The table contains a TableEntry array like keyboard bindings, but the "qualifier"
 * value is different.  For MIDI the only qualifier we need is the channel number.
 *
 */

#include <JuceHeader.h>

#include "util/Trace.h"
#include "util/Util.h"
#include "model/Symbol.h"
#include "model/MobiusConfig.h"
#include "model/UIConfig.h"
#include "model/UIAction.h"
#include "model/FunctionDefinition.h"
#include "model/UIParameter.h"
#include "model/Binding.h"
#include "model/ScriptProperties.h"

#include "KeyTracker.h"
#include "Supervisor.h"

#include "Binderator.h"

/**
 * Maximum index into a binding array.
 * One byte (256) is enough for MIDI and basic ASCII keys.
 * Don't need to mess it extended Unicode.
 *
 * OSC bindings will handle this in a different way.
 * Host parameter bindings are unclear, but I think we
 * can limit this to 256.
 */
const int BinderatorMaxIndex = 256;

//////////////////////////////////////////////////////////////////////
//
// Binderator
//
//////////////////////////////////////////////////////////////////////

Binderator::Binderator(Supervisor* s)
{
    supervisor = s;
    symbols = supervisor->getSymbols();
}

Binderator::~Binderator()
{
}

/**
 * Build out binding tables for both keyboard and MIDI events.
 */
void Binderator::configure(MobiusConfig* config)
{
    installKeyboardActions(config);
    installMidiActions(config);

    controllerThreshold = config->mControllerActionThreshold;
    if (controllerThreshold == 0)
      controllerThreshold = 127;
}

void Binderator::configureKeyboard(MobiusConfig* config)
{
    installKeyboardActions(config);
}

void Binderator::configureMidi(MobiusConfig* config)
{
    installMidiActions(config);
}

/**
 * Prepare a Juce::OwnedArray for use as a binding dispatch table.
 * Maximum number of events for each type is 256.
 *
 * This is annoying because even if you call ensureStorageAllocated
 * you can't just index into sparse arrays without setting something
 * there first, even if it is nullptr, if not it appends.
 * For some reason OwnedArray doesn't implement resized() like
 * Array does.
 *
 * Explore std::vector but I think it works the same.  Need to brush
 * up on sparse arrays, or just use a damn [] and be done with it.
 */
void Binderator::prepareArray(juce::OwnedArray<juce::OwnedArray<TableEntry>>* table)
{
    table->clear();
    for (int i = 0 ; i < BinderatorMaxIndex ; i++) {
        table->set(i, nullptr);
    }
}

/**
 * Add a table entry.
 * Be sure to call prepareArray on the table first.
 */
void Binderator::addEntry(juce::OwnedArray<juce::OwnedArray<TableEntry>>* table,
                          int hashKey,
                          unsigned int qualifier,
                          UIAction* action)
{
    juce::OwnedArray<TableEntry>* entries = (*table)[hashKey];
    if (entries == nullptr) {
        entries = new juce::OwnedArray<TableEntry>();
        table->set(hashKey, entries);
    }
    
    TableEntry* entry = new TableEntry();
    entry->qualifier = qualifier;
    entry->action = action;
    entries->add(entry);
}

/**
 * Look up an action in a table.
 * 
 * The optional wildZero argument is used only for MIDI bindings in
 * order to support the "any" binding channel.
 * Here the qualifier argument is the channel number of the incomming
 * message starting from 1.  An action matches if it has exactly the same
 * qualifier OR if the binding qualifier is zero.
 *
 * todo: with wildZero, there is a greater possibility of having multiple
 * actions bound to the same MIDI message but we will only return the first
 * one found in the entry list.  You could do that with specific channels
 * too, but it's less likely.  I don't know how useful having multiple
 * actions per trigger is, it's sort of like a macro, but you can't control
 * the order of evaluation.  If necessary this could be accomplished with
 * scripts, but reconsider someday.
 */
UIAction* Binderator::getAction(juce::OwnedArray<juce::OwnedArray<TableEntry>>* table,
                                int hashKey, unsigned int qualifier, bool wildZero)
{
    UIAction* action = nullptr;
    
    juce::OwnedArray<TableEntry>* entries = (*table)[hashKey];
    if (entries != nullptr) {
        for (auto entry : *entries) {
            if ((entry->qualifier == qualifier) ||
                (wildZero && entry->qualifier == 0)) {
                action = entry->action;
                break;
            }
        }
    }
    return action;
}

/**
 * Format the binding table entry qualifier for a KeyPress.
 * Juce key codes are for the most part ASCII except Fn keys,
 * arrows, Home/Ins/Del, etc.
 *
 * The convention it appears to follow is a 4 byte/32 bit
 * number where the bottom 16 bits are the usual character
 * numbers, with bit 17 on for "extended" characters.
 * Examples:
 *    dec hex mods getTextCharacter() getTextDescription() GetKeyString()
 *    65648 00010070 00000000 0 F1 F1
 *    65573 00010025 00000000 0 cursor left Left
 *    65582 0001002e 00000000 0 delete Delete
 *
 * Don't know if that's a standard or what, it is what it is.
 * I don't recall seeing any non-zero values in the second byte,
 * that is probably reserved for unicode.
 *
 * For the most part you can mask off the bottom byte and use
 * that as a table index, except for a few collisions:
 *
 *    x2e is . and delete
 *    x2d is - and insert
 *    x23 is # and end
 *    x24 is $ and home
 *
 * I didn't spend much time on the number pad because the Windows
 * did some bizarre interception of that which makes it unstable,
 * also the F11 and F12 keys were wonky, generating two KeyPresses.
 *
 * Whatever the encoding, since we need more than one byte to qualify
 * the key, the TableEntry has a full 32 bits of qualifier.
 * The bottom 17 bits of this I'm leaving as the Juce keyCode.
 * I did not in my testing see any use of bits above 17 so those are
 * used for the modifier keys.
 *
 * bit 18 - shift
 * bit 19 - ctrl
 * bit 20 - alt
 * bit 21 - command
 *
 * This is the bottom 4 bits of the juce::ModifierKeys.  You mostly don't
 * need the shift modifier since letter keys come in with different upper
 * and lower key codes.  I guess you could use this for Shift-UpArrow
 * if you wanted.  We have enough room for the three mouse button bits
 * but it's unclear when those would be set, and I don't want to confuse things.
 *
 * Note that this "qualifier" INCLUDES the bottom byte of the key code which
 * is also used as the table index.  So it's more than just the qualifier,
 * it's really the whole thing, and is the single value we can store
 * in the Binding model to represent this key.
 */
unsigned int Binderator::getKeyQualifier(int code, int modifiers)
{
    // start with the bottom 17 bits of the key code
    unsigned int qualifier = code & 0x1FFFF;
    // get the bottom 4 modifier bits
    unsigned int modBits = modifiers & 0x0F;
    // move them above the key code
    modBits = modBits << 17;
    
    qualifier |= modBits;

    return qualifier;
}

/**
 * Do the reverse, needed by KeyboardPanel to restore the original
 * Juce values when showing a text representation of the key.
 */
void Binderator::unpackKeyQualifier(int value, int* code, int* modifiers)
{
    if (code != nullptr) *code = value & 0x1FFFF;
    if (modifiers != nullptr) *modifiers = value >> 17;
}

unsigned int Binderator::getKeyQualifier(const juce::KeyPress& kp)
{
    return getKeyQualifier(kp.getKeyCode(), kp.getModifiers().getRawFlags());
}

/**
 * By comparison the MIDI qualifier is easy, it's just the channel number.
 */
unsigned int Binderator::getMidiQualifier(const juce::MidiMessage& msg)
{
    return (unsigned int)msg.getChannel();
}

/**
 * Locate the keyboard bindings and build the mapping table.
 *
 * We don't support swapping BindingSets yet, just take the default set.
 */
void Binderator::installKeyboardActions(MobiusConfig* config)
{
    prepareArray(&keyActions);
    
    BindingSet* baseBindings = config->getBindingSets();
    if (baseBindings != nullptr) {
        Binding* binding = baseBindings->getBindings();
        while (binding != nullptr) {
            if (binding->trigger == TriggerKey) {
                int code = binding->triggerValue;
                // could check the upper range too
                if (code <= 0) {
                    Trace(1, "Binderator: Ignoring Binding for %s with invalid value %ld\n",
                          binding->getSymbolName(), (long)code);
                }
                else {
                    // code is the full qualifier returned by getKeyQualifier
                    // mask off the bottom byte for the array index
                    UIAction* action = buildAction(binding);
                    if (action != nullptr) {
                        int index = code & 0xFF;
                        addEntry(&keyActions, index, code, action);
                    }
                }
            }
            binding = binding->getNext();
        }
    }
}

/**
 * Locate the MIDI bindings and build the mapping tables
 * for each MIDI event type.
 *
 * todo: Need some channel sensitivity options
 *
 *    open - install all channel-specific bindings and require that
 *      triggers have the matching channel
 *
 *    fixed - install only bindings for a specific channel and ignore
 *       triggers not on that channel
 *
 *    ignore - install only bindings for a specific channel (or all of them?)
 *       and ignore the trigger channel, matching only on the note number
 */
void Binderator::installMidiActions(MobiusConfig* config)
{
    prepareArray(&noteActions);
    prepareArray(&programActions);
    prepareArray(&controlActions);

    // we need this now, configure() shouldn't be requiring the objects
    // to be passed down any more, just pull what you need from Supervisor
    UIConfig* uiconfig = supervisor->getUIConfig();

    // always add base bindings
    BindingSet* baseBindings = config->getBindingSets();
    if (baseBindings != nullptr) {
        installMidiActions(baseBindings);
        // plus any active overlays
        BindingSet* overlay = baseBindings->getNextBindingSet();
        while (overlay != nullptr) {
            if (uiconfig->isActiveBindingSet(juce::String(overlay->getName()))) 
              installMidiActions(overlay);
            overlay = overlay->getNextBindingSet();
        }
    }
}

void Binderator::installMidiActions(BindingSet* set)
{
    Binding* binding = set->getBindings();
    while (binding != nullptr) {

        juce::OwnedArray<juce::OwnedArray<TableEntry>>* dest = nullptr;
        Trigger* trigger = binding->trigger;
        if (trigger == TriggerNote)
          dest = &noteActions;
        else if (trigger == TriggerProgram)
          dest = &programActions;
        else if (trigger == TriggerControl)
          dest = &controlActions;

        if (dest != nullptr) {

            int index = binding->triggerValue;
            if (index < 0 ||  index >= BinderatorMaxIndex) {
                Trace(1, "Binderator: Invalid MIDI note %s\n", binding->getSymbolName());
            }
            else {
                // todo: here is where we could be sensitive to a global option
                // to ignore channels, but it's less necessary now with the "Any"
                // channel in each binding
                UIAction* action = buildAction(binding);
                if (action != nullptr) {
                    // note that the Binding model uses MIDI channel 0 to mean
                    // "any" and specific channels are numbered from 1
                    // this needs to be understood when matching incomming events
                    int qualifier = binding->midiChannel;
                    addEntry(dest, index, qualifier, action);
                }
            }
        }
        binding = binding->getNext();
    }
}

/**
 * Given a key code, look up a corresponding UIAction.
 */
UIAction* Binderator::getKeyAction(int code, int modifiers)
{
    int index = code & 0xFF;
    int qualifier = getKeyQualifier(code, modifiers);
    
    return getAction(&keyActions, index, qualifier);
}

/**
 * Given a MIDI message, look up the corresponding action.
 * MidiMessage channels start at 1 with zero reserved for sysex.
 * This matches channel numbers in the Binding model.
 * A binding channel of zero means "any" so we use the
 * "wild zero" option to getAction.
 */
UIAction* Binderator::getMidiAction(const juce::MidiMessage& message)
{
    UIAction* action = nullptr;
    
    int qualifier = message.getChannel();
    
    if (message.isNoteOnOrOff()) {
        int index = message.getNoteNumber();
        action = getAction(&noteActions, index, qualifier, true);
    }
    else if (message.isProgramChange()) {
        int index = message.getProgramChangeNumber();
        action = getAction(&programActions, index, qualifier, true);
    }
    else if (message.isController()) {
        int index = message.getControllerNumber();
        action = getAction(&controlActions, index, qualifier, true);
    }

    return action;
}

//////////////////////////////////////////////////////////////////////
//
// Binding to Action Conversion
//
// This is where the dark magic starts to happen relating to sustainabilility
// and binding arguments.  It was unbelievabily complex in old code and strewn
// all over the place.  UIAction now has a far more streamlined structure but
// in the process we lost big chunks of it that need to eventually be moved here.
// 
// The end result is a UIAction that is relatively self contained without needing
// to carry around information about the binding trigger, binding arguments,
// ActionOperators, etc.  And it especially has a simpler and more obvious notion
// of what "sustaining" is.
//
//////////////////////////////////////////////////////////////////////

/**
 * Build an action from a binding if it looks valid.
 * Returns nullptr for malformed Bindings.
 *
 * Since we are not necessarily within the kernel, can't assume
 * access to an ActionPool.  Okay since the actions will be allocated
 * once and resused for each trigger.
 */
UIAction* Binderator::buildAction(Binding* b)
{
    UIAction* action = nullptr;

    const char* name = b->getSymbolName();
    if (name == nullptr) {
        Trace(1, "Binderator: Ignoring Binding with no name\n");
    }
    else {
        Symbol* symbol =  symbols->intern(name);
        if (!looksResolved(symbol)) {
            Trace(1, "Binderator: Binding to unresolved symbol %s\n",
                  symbol->getName());
            // go ahead and make it, it might be resolved later
        }

        action = new UIAction();
        action->symbol = symbol;

        action->setScope(b->getScope());
        
        // if the binding has a simple numeric argument, promote that
        // to the action value
        const char* args = b->getArguments();
        if (args != nullptr && IsInteger(args))
          action->value = ToInt(args);

        // also copy the entire string for a few things that pass names
        CopyString(b->getArguments(), action->arguments, sizeof(action->arguments));

        // new hack to disable quantization, ideally we would have any parameter
        // override here but only noQuantize is in the UIAction model
        // while we're hacking, might as well support "quantize=off" but we can't
        // do any of the other options
        if (StartsWithNoCase(action->arguments, "noquant") ||
            StringEqualNoCase(action->arguments, "quantize=off"))
          action->noQuantize = true;

        // determine sustainabillity of the trigger
        // to be sustainable it must have a unique id
        // so don't just blindly follow TriggerMode

        Trigger* trigger = b->trigger;
        TriggerMode* mode = b->triggerMode;
        int sustainId = -1;
        
        if (trigger == TriggerKey) {
            // these are implicitly sustainable, but I suppose you might
            // want to turn that OFF in some cases, so look at the mode if specified
            // in practice, this would be done by setting mode to Once
            if (mode == nullptr || mode == TriggerModeMomentary)
              sustainId = UIActionSustainBaseKey + b->triggerValue;
        }
        else if (trigger == TriggerNote) {
            if (mode == nullptr || mode == TriggerModeMomentary)
              sustainId = UIActionSustainBaseNote + b->triggerValue;
        }
        else if (trigger == TriggerControl) {
            // CC's can behave as sustainable if you adopt a value threshold
            // e.g. 0 for off and 127 for on or >64 for on, etc.
            // update: I used to require TriggerModeMomentary but setting that
            // is unreliable, and broke sus/long for almost everyone
            // TriggerMode isn't baked yet and you can't set it in the binding
            // windows so always assume sustainable
            if (mode == nullptr || mode == TriggerModeMomentary)
              sustainId = UIActionSustainBaseControl + b->triggerValue;
        }
        else if (trigger == TriggerHost) {
            // not sure how we're going to do this, they're similar
            // to TriggerControl
            // wait on those
            // update: same with TriggerControl, err on the side of assuming
            // that if you're bothering to binding a host parameter to a function
            // you will use it as a momentary switch
            // ugh...don't have unique ids for these though,
            // really need to define some base ranges for the various types,
            // buttons have an unknown base and will probably conflict with
            // host parameter ids
            //if (mode == nullptr || mode == TriggerModeMomentary)
            //sustainId = UIActionSustainBaseHost + b->triggerValue;
        }
        // TriggerProgram and TriggerPitch are unsustainable
        // TriggerUI and TriggerOsc won't be seen here

        // if we found an id, then this can be sustained
        bool sustainableTrigger = (sustainId >= 0);

        // sustainability is also impacted by the target
        // we wouldn't have to check this but we can avoid unnecessary
        // actions and long press tracking for functions that will just
        // end up ignoring up transitions
        // the core model is more complex than a the single flag we
        // have on FunctionDefinition but it shouldn't need to be

        bool sustainableTarget = false;

        // note this only works if the FunctionDefinition symbols are
        // interned BEFORE the Binderator is initialized
        if (symbol->function != nullptr) {
            sustainableTarget = symbol->function->sustainable;
        }
        else if (symbol->script != nullptr) {
            // this won't actually be set since ScriptCompler doesn't remember
            // if it had !sustain, should we assume this?
            sustainableTarget = symbol->script->sustainable;
        }

        // okay, if both sides get along, we'll let this be a sustaining action
        if (sustainableTrigger && sustainableTarget) {
            action->sustain = true;
            action->sustainId = sustainId;
        }
    }
    
    return action;
}

/**
 * Sanity check to see if the symbol actually does anything before
 * we install a binding for it.
 * This hacky and unreliable as Symbol behavior evolves, but catches
 * errors in early development.
 */
bool Binderator::looksResolved(Symbol* s)
{
    return (s->id > 0 ||
            s->variable != nullptr ||
            s->function != nullptr ||
            s->parameter != nullptr ||
            s->structure != nullptr ||
            s->sample != nullptr ||
            s->script != nullptr);
}

//////////////////////////////////////////////////////////////////////
//
// Binderator Execution
//
// This is the magic that responds to runtime key/midi events and decides
// if a UIAction should be sent.  Some of the logic, especically for MIDI bindings
// is closely related to the logic in buildAction so be careful.  When we receive
// a MIDI event, just because we have a UIAction in the tables for that event type
// doesn't necessarily mean we generate an action.  That will depend on other
// properties of the event itself.
//
// todo: See comments with installMidiActions for channel sensitivity options
// that should be supported
//
//////////////////////////////////////////////////////////////////////

/**
 * Given a MIDI message, return a UIAction if we need to propagate one.
 * This will be a UIAction stored in the mapping tables with runtime modifications.
 * Because we reuse the same UIAction for every event, it is expected that the UIAction
 * will be executed synchronously by the caller, and if execution needs to be deferred
 * it will make a copy.
 *
 * Action values
 *
 * Notes retain the value that was set from the binding argument if any.
 * We don't currently have a way to pass note velocity through, it was used
 * in the past for an obscure LoopSwitch feature that adjusted the output level.
 * Need to rethink this.
 * 
 * CCs must be able to send their ranged value for parameters.
 * But if we're bound to a function, they are expected to behave as a simple
 * on/off switch and the value isn't important.  In that case retain the
 * binding argument value.
 */
UIAction* Binderator::handleMidiEvent(const juce::MidiMessage& message)
{
    UIAction* send = nullptr;
    
    // look up the base action from the tables
    UIAction* action = getMidiAction(message);
    if (action != nullptr) {

        // reset these from the last time
        action->sustainEnd = false;

        if (message.isNoteOn() || message.isProgramChange()) {
            send = action;
        }
        else if (message.isNoteOff()) {
            // only send if we decided this was a sustainable action
            if (action->sustain) {
                action->sustainEnd = true;
                send = action;
            }
        }
        else if (message.isController()) {
            int ccvalue = message.getControllerValue();
            // here is where we could have this operate as an on/off
            // control for sustained functions by testing value thresholds
            // that has to be forced on right now with binding args, which
            // means no one will actuall use it, but allow for testing
            // "off" is relatively easy, a controller value of zero
            // "on" is harder because we can get a lot of values if this
            // is triggered by a continuous controller rather than a footswitch
            // programmed to send discrete CCs.  Since we don't keep state of
            // previous messages have to assume that one specific value means "on"
            SymbolBehavior behavior = action->symbol->behavior;
            if (behavior == BehaviorFunction) {
                if (ccvalue == 0) {
                    // send off only if we decided this was sustainable
                    if (action->sustain) {
                        action->sustainEnd = true;
                        send = action;
                    }
                }
                else if (ccvalue >= controllerThreshold) {
                    // it's "down"
                    send = action;
                }
            }
            else if (behavior == BehaviorScript) {
                // these are treated like functeions unless the
                // continuous flag is set
                if (action->symbol->script != nullptr &&
                    action->symbol->script->continuous) {

                    // does this actually work?
                    action->value = ccvalue;
                    send = action;
                }
                else {
                    // sustainable function
                    if (ccvalue == 0) {
                        if (action->sustain) {
                            action->sustainEnd = true;
                            send = action;
                        }
                    }
                    else if (ccvalue >= controllerThreshold) {
                        // it's "down"
                        send = action;
                    }
                }
            }
            else if (behavior == BehaviorParameter) {
                // always send the CC value through, ignore binding arguments
                // don't need to scale these
                action->value = ccvalue;
                send = action;
            }
            else if (behavior == BehaviorActivation) {
                // we're selecting among a small set of structures,
                // could scale out here or let the action handler do it
                // punt for now, treat this like a function and require
                // the structure ordinal to be passed as a binding arg
                if (ccvalue == 127) {
                    send = action;
                }
            }
            else {
                // BehaviorScript, BehaviorSample
                // these don't support sustain and you don't ordinarilly
                // bind them to CCs
                // treat them like functions
                if (ccvalue == 127)
                  send = action;
            }
        }
        else {
            // must be pitch wheel, these will need
            // more complex scaling since it isn't 0-127
            // punt for now
        }
    }

    return send;
}

/**
 * Decide whether to handle a keyboard event.
 */
UIAction* Binderator::handleKeyEvent(int code, int modifiers, bool up)
{
    UIAction* send = nullptr;
    
    UIAction* action = getKeyAction(code, modifiers);
    if (action != nullptr) {

        // reset state left over from the last event
        action->sustainEnd = false;

        if (!up) {
            // down transitions always send
            send = action;
        }
        else {
            // this is normally sent but if buildAction decided to
            // override sustainability, obey that
            if (action->sustain) {
                action->sustainEnd = true;
                send = action;
            }
        }
    }

    return send;
}

//////////////////////////////////////////////////////////////////////
//
// ApplicationBinderator
//
//////////////////////////////////////////////////////////////////////

ApplicationBinderator::ApplicationBinderator(Supervisor* super) : binderator(super)
{
    supervisor = super;
}

ApplicationBinderator::~ApplicationBinderator()
{
    supervisor->getKeyTracker()->removeListener(this);
    supervisor->getMidiManager()->removeListener(this);
}

void ApplicationBinderator::start()
{
    if (!started) {
        supervisor->getKeyTracker()->addListener(this);
        supervisor->getMidiManager()->addListener(this);
        started = true;
    }
}

void ApplicationBinderator::stop()
{
    if (started) {
        supervisor->getKeyTracker()->removeListener(this);
        supervisor->getMidiManager()->removeListener(this);
        started = false;
    }
}

void ApplicationBinderator::configure(MobiusConfig* config)
{
    binderator.configure(config);
}

void ApplicationBinderator::configureKeyboard(MobiusConfig* config)
{
    binderator.configureKeyboard(config);
}

/**
 * Handle notification from the KeyTracker.
 */
void ApplicationBinderator::keyTrackerDown(int code, int modifiers)
{
    if (started) {
        UIAction* action = binderator.handleKeyEvent(code, modifiers, false);
        if (action != nullptr)
          supervisor->doAction(action);
    }
}

/**
 * Handle a notification from KeyTracker when a key goes up.
 */
void ApplicationBinderator::keyTrackerUp(int code, int modifiers)
{
    if (started) {
        UIAction* action = binderator.handleKeyEvent(code, modifiers, true);
        if (action != nullptr)
          supervisor->doAction(action);
    }
}

/**
 * Handle notification of a MIDI event
 * Anything useful in "source"?
 */
void ApplicationBinderator::midiMessage(const juce::MidiMessage& message, juce::String& source)
{
    (void)source;
    if (started) {
        UIAction* action = binderator.handleMidiEvent(message);
        if (action != nullptr)
          supervisor->doAction(action);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
