/**
 * A library of functions that may be directly called from MSl scripts
 * rather than using Symbols and UIActions.
 *
 * Unlike action functions, these support complex argument lists
 * and return values.
 *
 * Supervisor resolves named references to these and creates an MslExternal
 * that identifies the function with an id within the ScriptExternalId enumeration.
 */
 
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../util/Util.h"
#include "../midi/MidiByte.h"

#include "../Supervisor.h"
#include "../mobius/MobiusKernel.h"

#include "MslContext.h"
#include "MslExternal.h"

#include "ScriptExternals.h"

/**
 * Struct defining the association of a external name and the numeric id.
 * Used when resolving name references from a script.
 * Order is not significant.
 */
ScriptExternalDefinition ScriptExternalDefinitions[] = {

    {"MidiOut", ExtMidiOut, ScriptContextNone},
    {"GetMidiDeviceId", ExtGetMidiDeviceId, ScriptContextNone},

    {nullptr, ExtNone, ScriptContextNone}

};

/**
 * Map a function name into an internal id.
 * todo: if this starts getting large, use a HashMap
 */
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

/**
 * Eventually called by Supervisor in response to an action
 * generated by the script session.  The context here may
 * either be Supervisor or MobiusKernel since we don't force
 * a side.
 */
bool ScriptExternals::doAction(MslContext* c, MslAction* action)
{
    bool success = false;

    int intId = action->external->id;
    
    if (intId >= 0 && intId < (int)ExtMax) {
        ScriptExternalId id = (ScriptExternalId)intId;

        switch (id) {
            
            case ExtMidiOut:
                success = MidiOut(c, action);
                break;
            case ExtGetMidiDeviceId:
                success = GetMidiDeviceId(c, action);
                break;

            default:
                Trace(1, "ScriptExternals: Unhandled external id %d", id);
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

/**
 * Assemble a MIDI event from action arguments.
 * A very similar dance is done down in mobius/core/functions/Midi.cpp
 * except that it uses ExValueList rather than an MslValue list.
 * Consider finding a way to share.
 *
 * The old format was this, best to keep it the same;
 *
 * MidiOut <status> <channel> <value> <velocity>
 * status: noteon noteoff control program 
 * channel: 0-15
 * value: 0-127
 * velocity: 0-127
 *
 * I'd like to make some of these optional but it's hard without using
 * keyword arguments.
 *
 * To those I would like to add deviceId, but this will conflict with
 * velocity if you stick it on the end.  If we put it at the beginning e
 * can treat it like an alternative function signature.  If the first argument
 * is an integer, then it must be a device id.  To use symbolic device names
 * they must call GetMidiDevice(name)
 */
bool ScriptExternals::assembleMidiMessage(MslAction* action, juce::MidiMessage& msg,
                                          bool* returnSync,
                                          int *returnDeviceId)
{
    bool failed = false;
    int deviceId = -1;
	int status = 0;
	int channel = 0;
	int number = 0;
	int velocity = 0;
    bool isSync = false;

    MslValue* arg = action->arguments;
    if (arg == nullptr) {
        Trace(1, "MidiOut: No function arguments");
        failed = true;
    }
    else if (arg->type == MslValue::Int) {
        deviceId = arg->getInt();
        arg = arg->next;
    }
    else if (arg->isNull()) {
        // most likely an initialized variable that was supposed
        // to have a deviceId, really anything other than a String is considered
        // the deviceId
        Trace(1, "MidiOut: Device id argument was null");
        arg = arg->next;
    }

    //
    // In retrospect, it would be a lot easier for externals to deal with
    // an Array of arguments rather than a linked list.  Easier to access
    // and reason about.
    //

    if (!failed) {
        if (arg == nullptr) {
            Trace(1, "MidiOut: Missing message type");
        }
        else {
            const char* type = arg->getString();
            if (StringEqualNoCase(type, "note") ||
                StringEqualNoCase(type, "noteon")) {
                status = MS_NOTEON;
                velocity = 127;
            }
            else if (StringEqualNoCase(type, "noteoff")) {
                status = MS_NOTEOFF;
            }
            else if (StringEqualNoCase(type, "control") ||
                     StringEqualNoCase(type, "cc")) {
                status = MS_CONTROL;
            }
            else if (StringEqualNoCase(type, "program") ||
                     StringEqualNoCase(type, "pgm")) {
                status = MS_PROGRAM;
            }
            // these are occasionally necessary
            else if (StringEqualNoCase(type, "start")) {
                status = MS_START;
                isSync = true;
            }
            else if (StringEqualNoCase(type, "continue")) {
                status = MS_CONTINUE;
                isSync = true;
            }
            else if (StringEqualNoCase(type, "stop")) {
                status = MS_STOP;
                isSync = true;
            }
            else if (StringEqualNoCase(type, "clock")) {
                status = MS_CLOCK;
                isSync = true;
            }
            // these are almost never used, and I probably have parsing wrong anyway
            else if (StringEqualNoCase(type, "poly")) {
                status = MS_POLYPRESSURE;
            }
            else if (StringEqualNoCase(type, "touch")) {
                status = MS_TOUCH;
            }
            else if (StringEqualNoCase(type, "bend")) {
                status = MS_BEND;
            }
            else {
                Trace(1, "MidiOut: Invalid status %s\n", type);
                failed = true;
            }
            arg = arg->next;
        }
    }

    // second argument is the channel
    if (!failed && !isSync) {
        if (arg == nullptr) {
            Trace(1, "MidiOut: Missing message channel");
            failed = true;
        }
        else {
            channel = arg->getInt();
            arg = arg->next;
        }
    }

    // third argument is the note/program/control number
    if (!failed && !isSync) {
        if (arg == nullptr) {
            Trace(1, "MisiOut: Missing message number");
            failed = true;
        }
        else {
            number = arg->getInt();
            arg = arg->next;
        }
    }
    
    // final argument is the optional velocity
    if (!failed && !isSync) {
        if (arg != nullptr) {
            velocity = arg->getInt();
            arg = arg->next;
        }
    }
    
    if (!failed && arg != nullptr) {
        // not really a problem, but they probably did something wrong
        Trace(1, "MidiOut: Ignoring extra arguments");
    }
    
    if (!failed) {

        int juceChannel = channel + 1;
        
        switch (status) {
            
            case MS_NOTEON:
                msg = juce::MidiMessage::noteOn(juceChannel, number, (juce::uint8)velocity);
                break;
                
            case MS_NOTEOFF:
                msg = juce::MidiMessage::noteOff(juceChannel, number, (juce::uint8)velocity);
                break;

            case MS_PROGRAM:
                msg = juce::MidiMessage::programChange(juceChannel, number);
                break;

            case MS_CONTROL:
                msg = juce::MidiMessage::controllerEvent(juceChannel, number, (juce::uint8)velocity);
                break;

            case MS_CLOCK:
            case MS_START:
            case MS_STOP:
            case MS_CONTINUE: {
                msg = juce::MidiMessage(status, 0, 0);
            }

            default: {
                // punt and hope the 3 byte constructor is smart enough to figure out how
                // many bytes the status actually needs
                // todo: test this
                int byte1 = status | channel;
                msg = juce::MidiMessage(byte1, number, (juce::uint8)velocity);
            }
        }

        if (returnSync != nullptr)
          *returnSync = isSync;

        if (returnDeviceId != nullptr)
          *returnDeviceId = deviceId;
    }

    return !failed;
}


/**
 * Send a MIDI event.
 *
 * The script will most often be an event script running in the kernel.
 * This raises an interesting issue about what to do if we are in the
 * UI instead and running as a plugin.  If there are no direct MIDI
 * devices open, then MIDI is sent through Juce during audio block processing.
 * There isn't a mechanism for the UI to send the event down to the Kernel
 * to include on the next block.  There should be but it isn't likely to happen.
 * This does argue for this to be a Kernel context function to force a thread
 * transition if it happens.
 */
bool ScriptExternals::MidiOut(MslContext* c, MslAction* action)
{
    juce::MidiMessage msg;
    int deviceId = -1;
    bool isSync = false;

    bool success = assembleMidiMessage(action, msg, &isSync, &deviceId);

    if (success) {
    
        if (c->mslGetContextId() == MslContextShell) {
            Supervisor* s = static_cast<Supervisor*>(c);
            if (deviceId == -1) {
                // todo: check if we can even do this and warn
                if (isSync)
                  s->midiSendSync(msg);
                else
                  s->midiExport(msg);
            }
            else {
                s->midiSend(msg, deviceId);
            }
        }
        else {
            MobiusKernel* kernel = static_cast<MobiusKernel*>(c);
            if (deviceId == -1) {
                if (isSync)
                  kernel->midiSendSync(msg);
                else
                  kernel->midiSendExport(msg);
            }
            else {
                kernel->midiSend(msg, deviceId);
            }
        }
    }
    
    return success;
}

/**
 * Get the internal numeric device identifier for a device name
 */
bool ScriptExternals::GetMidiDeviceId(MslContext* c, MslAction* action)
{
    bool success = false;

    MslValue* arg = action->arguments;
    if (arg == nullptr) {
        Trace(1, "GetMidiDeviceId: No arguments");
    }
    else if (arg->type != MslValue::String) {
        Trace(1, "GetMidiDeviceId: Name argument not a string");
    }
    else {
        int deviceId = -1;

        if (c->mslGetContextId() == MslContextShell) {
            Supervisor* s = static_cast<Supervisor*>(c);
            deviceId = s->getMidiOutputDeviceId(arg->getString());
        }
        else {
            MobiusKernel* kernel = static_cast<MobiusKernel*>(c);
            deviceId = kernel->getMidiOutputDeviceId(arg->getString());
        }

        if (deviceId == -1) {
            Trace(1, "GetMidiDeviceId: Invalid device name %s", arg->getString());
        }
        else {
            success = true;
        }

        // whether succesfull or not return -1 in case they want to deal with that?
        action->result.setInt(deviceId);
    }
    
    return success;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

