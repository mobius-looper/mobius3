/**
 * Singleton class that manages MIDI devices for an application.
 * Constructed, owned, and accessed through Supervisor.
 * 
 * Initially adapted from the HandlingMidiEvents tutorial.
 * After some flailing around thinking AudioDeviceManager needed to
 * be in the middle of everything I think it doesn't.  It provides
 * some slightly useful tracking of listeners where you don't really
 * care what the devices are and they can be selected or disabled in
 * uncontrolled ways.
 *
 * I don't really have this issue so direct access to the MidiInput
 * and MidiOutput classes is enough.  Some code below that uses
 * AudioDeviceManager to open input devices is retained for reference
 * but not used.
 *
 * Juce docs on how MIDI devices are managed are vague and confusing and
 * different for direct device access vs. going through AudioDeviceManager.
 * Some of the comments below reflect initial misunderstandings and need to
 * be cleaned up.  It seems to work like this...
 *
 * What "open" means is much lighter weight than audio devices.  Opening
 * a MidiInput means that a callback is registered to receive notifications of
 * incomming MidiMessages and the device is "started".  The terms when going through
 * AudioDeviceManager are different, you open the device with a callback and "enable".
 * I think "enable" and "start" are the same.
 *
 * There is no "close" method.  When you open a device you get a unique_ptr to a
 * MidiInput or MidiOutput and closing it is deleting those objects.  This appears
 * to be the mechanism to unregister a MidiInput callback.  You can also "start" and
 * "stop" them, but this leaves them in an "open" state with callback in place, it just
 * won't receive any messages.
 *
 * If you're using AudioDeviceManager, this has a removeMidiDeviceCallback but
 * it's unclear whether the device is still internally connected, which can be important
 * if you're fighting with the host over who gets to connect to a device.
 *
 * So it is probably important to delete the MidiInput objects when no longer needed.
 *
 * MidiOutput has no callback, you just get one and use it.  There is no close
 * or "stop" like inputs, I guess you just delete it when you're done.
 *
 * For standalone Mobius, the devices to open are stored in devices.xml which supports
 * configurations for multiple machines.  This is something I use all the time during
 * development with .xml files that are in source control, but normal users will normally
 * have only one machine.
 *
 * What the configurations mean...
 *
 * This was originally more restrictive about multiple devices being open at a time.
 * With the introduction of MidiTracks it is more relaxed and some of the logic might
 * be confusing.   Basically you can have any number of input and output devices selected.
 *
 * In addition you can select inputs and outputs for a paricular "usage".  This is what the
 * system uses to determine which one of several devices is to be used for some purpose
 * like MIDI export, synchronization, or Thru.  For a given usage there can only be one selection.
 *
 * What is different is that before there could only be a single output selected for any purpose,
 * now there can be multiples.  The code also makes it look like an input/output can be deselected
 * but selected for a particular use.  That isn't true, if you select any usage, it is always selected
 * for general use.
 *
 * In the UI it might be clearer to skip the checkbox wall and instead just allow selecting multiple
 * input and output devices.  Then for usage provide a combo box to select which of the opened
 * device should be used for that purpose.
 *
 * Here we're reinforcing this by making sure the inputDevices and outputDevices list contain the
 * names of all devices we're opening, whether or not they are selected for a usage.
 */

#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/DeviceConfig.h"
#include "model/Session.h"

#include "Supervisor.h"
#include "MidiManager.h"

MidiManager::MidiManager(Supervisor* super)
{
    supervisor = super;

    // an example did this to show relative arrival time of logged messages
    // "Returns the number of millisecs since a fixed event (usually system startup).
    // This has the same function as getMillisecondCounter(), but returns a more accurate
    // value, using a higher-resolution timer if one is available."
    // jsl - the divide by 1000 is done to match the same calculation
    // done to MidiMessage::getTimeStamp when created by MidiInput
    // " The message's timestamp is set to a value equivalent to (Time::getMillisecondCounter() / 1000.0) to specify the time when the message arrived"
    startTime = (juce::Time::getMillisecondCounterHiRes() * 0.001);
}

MidiManager::~MidiManager()
{
    shutdown();
}

/**
 * Called during initialization and after modifying the Session
 * We watch the allocation of MIDI tracks, and if there are any enable recording.
 */
void MidiManager::configure()
{
    Session* session = supervisor->getSession();
    // assume for now that if there are any they're midi
    if (session->getMidiTracks() > 0)
      recordable = true;
}

/**
 * Stop the MIDI input callback.
 * This is presumably necessary if you edit bindings and need to rebuild
 * the Binderator.  We don't want messages comming in on another thread
 * that would be confused by a partially constructed Binderator.
 * Might not be a problem if the editing is done on the UI thread and the
 * callback also uses the message thread.
 */
void MidiManager::suspend()
{
    stopInputs();
}

/**
 * Re-open just the input devices after suspend()
 * Since the inputDevices list is never pruned, have to let DeviceConfig
 * be the guide for what gets restarted.
 */
void MidiManager::resume()
{
    startInputs();
}

/**
 * "Close" any open devices.
 * I don't think we really need to do anything here.  Juce will clean up whatever
 * system resources were allocated, the only thing that might be important is
 * unregistering our MidiInput callbacks since destructor order is unclear.
 */
void MidiManager::shutdown()
{
    closeAllInputs();
    closeAllOutputs();
    listeners.clear();
    realtimeListeners.clear();
}

void MidiManager::addListener(Listener* l)
{
    if (!listeners.contains(l))
        listeners.add(l);
}

void MidiManager::removeListener(Listener* l)
{
    listeners.removeAllInstancesOf(l);
}

void MidiManager::addRealtimeListener(RealtimeListener* l)
{
    if (!realtimeListeners.contains(l))
        realtimeListeners.add(l);
}

void MidiManager::removeRealtimeListener(RealtimeListener* l)
{
    realtimeListeners.removeAllInstancesOf(l);
}

void MidiManager::addMonitor(Monitor* l)
{
    if (!monitors.contains(l))
      monitors.add(l);
}

void MidiManager::removeMonitor(Monitor* l)
{
    monitors.removeAllInstancesOf(l);
}

/**
 * Say something bad...
 *
 * Trace so we can see it immediately the debug log if nothing is watching.
 * Add it to the error list so we can show it the next time the monitor window
 * is opened. And finally send it to any monitors that might be open now.
 */
void MidiManager::somethingBadHappened(juce::String msg)
{
    Trace(1, "MidiManager: %s\n", msg.toUTF8());
    errors.add(msg);
    monitorMessage(msg);
}

void MidiManager::monitorMessage(juce::String msg)
{
    for (auto monitor : monitors)
      monitor->midiMonitorMessage(msg);
}

/**
 * Open previously configured devices.
 * This is always called at startup, and may be called by MidiDevicesPanel
 * randomly to reflect changes made in the UI.
 *
 * Not distinguishing between input "control" and "sync" devices, they'll all
 * get opened with the same callback.  Unclear if there is any benefit to
 * having different callbacks for the two different usages.  Internal listeners
 * don't care, they'll take events from wherever they come.  I suppose you could
 * want to designate a device for sync input that may receive MIDI commands you
 * don't want sent through Binderator.
 *
 * Output control vs. sync is important however.  MidiOut from scripts and MIDI
 * state export will go to the control output device, and synchronization messages
 * will goto the sync device if there is one.
 */
void MidiManager::openDevices()
{
    DeviceConfig* config = supervisor->getDeviceConfig();
    MachineConfig* mconfig = config->getMachineConfig();

    // keep a list of device errors for display at an appropriate time later
    errors.clear();

    reconcileInputs(mconfig);
    reconcileOutputs(mconfig);

}

/**
 * Return the device list (a csv) for the given usage, depending on whether or not we
 * are running as a plugin.
 *
 * The two general MIDI inputs are allowed to be a csv, the others must
 * all be singles, and if misconfigured, returns the first one.
 */
juce::String MidiManager::getDeviceName(MachineConfig* config, Usage usage)
{
    juce::String name;
    
    if (supervisor->isPlugin()) {
        
        if (usage == Input)
          name = config->pluginMidiInput;
        
        else if (usage == InputSync)
          name = getFirstName(config->pluginMidiInputSync, usage);
        
        else if (usage == Output)
          name = config->pluginMidiOutput;
        
        else if (usage == Export)
          name = getFirstName(config->pluginMidiExport, usage);
        
        else if (usage == OutputSync)
          name = config->pluginMidiOutputSync;
        
        else if (usage == Thru)
          name = getFirstName(config->pluginMidiThru, usage);
    }
    else {
        if (usage == Input)
          name = config->midiInput;
        
        else if (usage == InputSync)
          name = getFirstName(config->midiInputSync, usage);
        
        else if (usage == Output)
          name = config->midiOutput;
        
        else if (usage == Export)
          name = getFirstName(config->midiExport, usage);
        
        else if (usage == OutputSync)
          name = config->midiOutputSync;
        
        else if (usage == Thru)
          name = getFirstName(config->midiThru, usage);
    }

    return name;
}

/**
 * Return a displable string for a Usage
 */
juce::String MidiManager::getUsageName(Usage usage)
{
    juce::String name;
    switch (usage) {
        case Input: name = "Input"; break;
        case InputSync: name = "Input Sync"; break;
        case Output: name = "Output"; break;
        case Export: name = "Export"; break;
        case OutputSync: name = "Output Sync"; break;
        case Thru: name = "Thu"; break;
    }
    return name;
}

/**
 * We have allowed devices.xml to have lists of names, though that
 * should have been prevented for output devices.  Currently only
 * allowing one output device at a time since there isn't a way
 * to address them from within.
 */
juce::String MidiManager::getFirstName(juce::String csv, Usage usage)
{
    juce::String name;
    juce::StringArray list = juce::StringArray::fromTokens(csv, ",", "");

    if (list.size() > 1) {
        juce::String uname = getUsageName(usage);
        juce::String msg = "Multiple " + uname + " devices configured but only one can be opened:" + csv;
        somethingBadHappened(msg);
    }
    
    if (list.size() > 0)
      name = list[0];
    return name;
}

//////////////////////////////////////////////////////////////////////
//
// Input Devices
//
//////////////////////////////////////////////////////////////////////

/**
 * Open the output devices configured, and close the ones that are not.
 */
void MidiManager::reconcileInputs(MachineConfig* config)
{
    juce::String csv = getDeviceName(config, Input);
    inputNames = juce::StringArray::fromTokens(csv, ",", "");
    
    // upgrade old configurations that kept this out of the main name list
    juce::String syncName = getDeviceName(config, InputSync);
    if (syncName.length() > 0 && !inputNames.contains(syncName))
      inputNames.add(syncName);

    for (auto name : inputNames)
      (void)findOrOpenInput(name);

    // get a handle to the special sync input
    if (syncName.length() > 0)
      inputSyncDevice = findInput(syncName);

    closeUnusedInputs();
}

juce::MidiInput* MidiManager::findInput(juce::String name)
{
    juce::MidiInput* found = nullptr;
    for (auto dev : inputDevices) {
        if (dev->getName() == name) {
            found = dev;
            break;
        }
    }
    return found;
}

/**
 * Open an input device if not already open.
 * This is called both at startup when loading devices.xml and can also
 * be called randomly by MidiDevicesPanel.
 *
 * What "open" means in Juce is weird.  "open" gets you a handle to a MidiInput
 * object inside a std::unique_ptr so it apparently expects those to be deleted
 * when you're done.  Once you have it open you start and stop it.  There
 * doesn't appear to be a "close", in testing, deleting the object seemed to
 * deregister the callback.
 */
juce::MidiInput* MidiManager::findOrOpenInput(juce::String name)
{
    juce::MidiInput* found = findInput(name);
    if (found != nullptr) {
        // already open, unclear what should happen if this was
        // in a stopped state, which shouldn't happen from the devices panel
        // I guess just ensure it is started so we can monitor it
        found->start();
    }
    else if (name.length() > 0) {
        juce::String id = getInputDeviceId(name);
        if (id.isEmpty()) {
            juce::String msg = "Unable to find input device id for " + name;
            somethingBadHappened(msg);
        }
        else {
            monitorMessage("Opening input " + name);

            std::unique_ptr<juce::MidiInput> dev = juce::MidiInput::openDevice(id, this);
            if (dev == nullptr) {
                juce::String msg = "Unable to open input " + name;
                somethingBadHappened(msg);
            }
            else {
                found = dev.release();
                inputDevices.add(found);
                // presumably this is what starts it pumping events to the callback
                found->start();
            }
        }
    }
    return found;
}

/**
 * Open an input device if not already open.
 * This is designed to be called from MidiDevicesPanel when aa device is checked.
 * The device is always added to the main input list, opened for the usage,
 * and saved in the usage pointer.
 */
void MidiManager::openInput(juce::String name, Usage usage)
{
    if (!inputNames.contains(name))
      inputNames.add(name);
    (void)findOrOpenInput(name);
    
    if (usage == InputSync) {
        inputSyncDevice = findOrOpenInput(name);
        // change the sync device doesn't automatically unselect it
        // for general use any more, so this should do nothing
        closeUnusedInputs();
    }
}

/**
 * Close one of the input devices.
 * Removing a usage doesn't remove the device from the main list.
 *
 * If you deselect one of the main devices, it is also deselected
 * for any usages.
 */
void MidiManager::closeInput(juce::String name, Usage usage)
{
    if (usage == Input) {
        inputNames.removeString(name);
        
        if (inputSyncDevice != nullptr && inputSyncDevice->getName() == name)
          inputSyncDevice = nullptr;
    }
    else if (usage == InputSync) {
        if (inputSyncDevice != nullptr && inputSyncDevice->getName() == name)
          inputSyncDevice = nullptr;
    }
    closeUnusedInputs();
}

void MidiManager::closeUnusedInputs()
{
    int index = 0;
    while (index < inputDevices.size()) {
        juce::MidiInput* dev = inputDevices[index];
        if (inputNames.contains(dev->getName())) {
            // still need this one
            index++;
        }
        else {
            // close and delete this one
            monitorMessage("Closing input " + dev->getName());
            dev->stop();
            if (dev == inputSyncDevice)
              inputSyncDevice = nullptr;
            inputDevices.removeObject(dev, true);
        }
    }
}

/**
 * Stop any currently open input devices.
 */
void MidiManager::stopInputs()
{
    for (auto dev : inputDevices) {
        dev->stop();
    }
}    

/**
 * Restart any currently open input devices.
 */
void MidiManager::startInputs()
{
    for (auto dev : inputDevices) {
        dev->start();
    }
}    

void MidiManager::closeAllInputs()
{
    for (auto dev : inputDevices)
      monitorMessage("Closing input " + dev->getName());
        
    // stop them first?
    stopInputs();
    
    // this deletes them
    inputDevices.clear();
}    

//////////////////////////////////////////////////////////////////////
//
// Output Devices
//
//////////////////////////////////////////////////////////////////////

/**
 * Open the output devices configured, and close the ones that are not.
 *
 * Formerly allowed only one output device and had a complicated dependency
 * on the specific use devices.  Now there is just a list of devices to
 * open, and if a usage device isn't on the main list it is added back.
 *
 * The rules here are subtle and different from what they first were.
 * The MidiDeviceEditor should never assume anything about which devices
 * are open or closed after asking MidiManager to do anything because
 * I don't want to keep the rules in sync.
 */
void MidiManager::reconcileOutputs(MachineConfig* config)
{
    juce::String csv = getDeviceName(config, Output);
    outputNames = juce::StringArray::fromTokens(csv, ",", "");
    
    juce::String exportName = getDeviceName(config, Export);
    juce::String syncCsv = getDeviceName(config, OutputSync);
    juce::StringArray syncNames = juce::StringArray::fromTokens(syncCsv, ",", "");
    juce::String thruName = getDeviceName(config, Thru);
    
    // upgrade old configurations to put the usage-specific devices
    // on the main list
    if (exportName.length() > 0 && !outputNames.contains(exportName))
      outputNames.add(exportName);

    for (auto name : syncNames) {
        if (!outputNames.contains(name))
          outputNames.add(name);
    }
    
    //if (syncName.length() > 0 && !outputNames.contains(syncName))
    //outputNames.add(syncName);
    
    if (thruName.length() > 0 && !outputNames.contains(thruName))
      outputNames.add(thruName);

    // open them all
    for (auto name : outputNames) {
        (void)findOrOpenOutput(name);
    }

    // get device handles for the usages
    openOutputInternal(exportName, Export);
    
    for (auto name : syncNames) {
        openOutputInternal(name, OutputSync);
    }
    
    openOutputInternal(thruName, Thru);
    
    closeUnusedOutputs();
}

/**
 * Open a device for a particular usage.
 * This is intended for use by the MidiDevicesPanel when checking a box in the grid.
 * The device is added to the main output list if it isn't already there and assigned
 * to the usage handle after it is opened.
 */
void MidiManager::openOutput(juce::String name, Usage usage)
{
    // always goes on the master list
    if (!outputNames.contains(name))
      outputNames.add(name);
    
    openOutputInternal(name, usage);
    // this won't actually close anything now since changing a usage
    // doesn't mean it is taken off the main list
    closeUnusedOutputs();
}

/**
 * Open a device for a particular usage but do not close unused outputs yet.
 */
void MidiManager::openOutputInternal(juce::String name, Usage usage)
{
    if (usage == Output) {
        // just a general unspecific output
        (void)findOrOpenOutput(name);
    }
    else if (usage == Export) {
      exportDevice = findOrOpenOutput(name);
    }
    else if (usage == OutputSync) {
        juce::MidiOutput* dev = findOrOpenOutput(name);
        if (dev != nullptr) {
            if (!outputSyncDevices.contains(dev))
              outputSyncDevices.add(dev);
        }
    }
    else if (usage == Thru) {
        thruDevice = findOrOpenOutput(name);
    }
}

/**
 * Close the output device with the given usage if it not used for something else.
 * This works differently than the first release, closing a usage device won't
 * take it off the main list.
 *
 * This could work two ways (so could inputs).
 * If you ask to close one of the general Output devices it could automatically
 * make that unavailable for the other usages.  Or if the device is still selected
 * for a usage, it prevents the output from being closed.
 * Not sure which is less confusing.
 */
void MidiManager::closeOutput(juce::String name, Usage usage)
{
    if (usage == Output) {

        if (outputNames.contains(name)) {
            outputNames.removeString(name);
            
            // method 2, deselecting it from the main list, deselects it from all usages
            if (exportDevice != nullptr && exportDevice->getName() == name)
              exportDevice = nullptr;
        
            if (thruDevice != nullptr && thruDevice->getName() == name)
              thruDevice = nullptr;

            (void)removeOutputSyncDevice(name);
            
            closeUnusedOutputs();
        }
    }
    else if (usage == Export) {
        if (exportDevice != nullptr && exportDevice->getName() == name) {
            exportDevice = nullptr;
            closeUnusedOutputs();
        }
    }
    else if (usage == OutputSync) {
        if (removeOutputSyncDevice(name))
          closeUnusedOutputs();
    }
    else if (usage == Thru) {
        if (thruDevice != nullptr && thruDevice->getName() == name) {
            thruDevice = nullptr;
            closeUnusedOutputs();
        }
    }
}

bool MidiManager::removeOutputSyncDevice(juce::String name)
{
    bool removed = false;
    for (auto dev : outputSyncDevices) {
        if (dev->getName() == name) {
            outputSyncDevices.removeAllInstancesOf(dev);
            removed = true;
            break;
        }
    }
    return removed;
}

/**
 * Find an output device that has already been opened.
 */
juce::MidiOutput* MidiManager::findOutput(juce::String name)
{
    juce::MidiOutput* found = nullptr;
    for (auto dev : outputDevices) {
        if (dev->getName() == name) {
            found = dev;
            break;
        }
    }
    return found;
}

/**
 * Open an output device if it is not already open.
 * Return either the new one or the an existing one.
 */
juce::MidiOutput* MidiManager::findOrOpenOutput(juce::String name)
{
    juce::MidiOutput* found = findOutput(name);
    if (found == nullptr) {
        if (name.length() > 0) {
            juce::String id = getOutputDeviceId(name);

            if (id.isEmpty()) {
                juce::String msg = "Unable to find output device id for " + name;
                somethingBadHappened(msg);
            }
            else {
                monitorMessage("Opening output " + name);
                std::unique_ptr<juce::MidiOutput> dev = juce::MidiOutput::openDevice(id);
                if (dev == nullptr) {
                    juce::String msg = "Unable to open output " + name;
                    somethingBadHappened(msg);
                }
                else {
                    // liberate it
                    found = dev.release();
                    outputDevices.add(found);
                }

            }
        }
    }

    // since 
    
    return found;
}

/**
 * Close any open output devices that are not selected for a usage.
 */
void MidiManager::closeUnusedOutputs()
{
    int index = 0;
    while (index < outputDevices.size()) {
        juce::MidiOutput* dev = outputDevices[index];
        if (outputNames.contains(dev->getName())) {
            // still need this one
            index++;
        }
        else {
            // close and delete this one
            monitorMessage("Closing output " + dev->getName());
            
            if (dev == exportDevice) exportDevice = nullptr;
            outputSyncDevices.removeAllInstancesOf(dev);
            if (dev == thruDevice) thruDevice = nullptr;
            
            outputDevices.removeObject(dev, true);
        }
    }
}

/**
 * Close all open output devices and reset the usage pointers
 */
void MidiManager::closeAllOutputs()
{
    for (auto dev : outputDevices)
      monitorMessage("Closing output " + dev->getName());

    outputDevices.clear();
    exportDevice = nullptr;
    outputSyncDevices.clear();
    thruDevice = nullptr;
}

/**
 * Return the name of the currently open output devices.
 * Used by MidiDevicesPanel to show what we were able to do after the
 * last openDevices.
 */
juce::StringArray MidiManager::getOpenOutputDevices()
{
    juce::StringArray names;
    for (auto dev : outputDevices)
      names.add(dev->getName());
    return names;
}

//////////////////////////////////////////////////////////////////////
//
// Internal Device Ids
//
// For messages that pass into and out of the engine, I want to refer
// to devices using a simple numeric identiifer that is easy to embed
// and copy without comparing long strings, allocating memory for strings,
// or directly touching the MIDI device object pointers.
//
// This id is assigned on startup as devices are opened, but once assigned
// needs to be stable for the duration of the engine.  Reconfiguring
// by opening and closing devices should not disrupt this making indexes
// into the inputDevices and outputDevices arrays unusable.
//
// MIDI track configuration will save device references by name, so on startup
// and after MIDI reconfiguration a mapping between names and ids is needed.
//
//////////////////////////////////////////////////////////////////////

// lots to do here...

//////////////////////////////////////////////////////////////////////
//
// Output Messages
//
//////////////////////////////////////////////////////////////////////

bool MidiManager::hasOutputDevice(Usage usage)
{
    bool has = false;

    if (usage == Export)
      has = (exportDevice != nullptr);
    
    else if (usage == OutputSync)
      has = (outputSyncDevices.size() > 0) || (exportDevice != nullptr);
    
    else if (usage == Thru)
      has = (thruDevice != nullptr);

    return has;
}

void MidiManager::send(const juce::MidiMessage& msg, int deviceId)
{
    if (deviceId >= 0 && deviceId < outputDevices.size()) {
        juce::MidiOutput* dev = outputDevices[deviceId];
        dev->sendMessageNow(msg);
    }
}

void MidiManager::sendExport(const juce::MidiMessage& msg)
{
    if (exportDevice)
      exportDevice->sendMessageNow(msg);
    else {
        // what if they didn't check one, default to the first output?
        // this is unlike sync which doesn't default
        if (outputDevices.size() > 0) {
            juce::MidiOutput* dev = outputDevices[0];
            dev->sendMessageNow(msg);
        }
    }
}

void MidiManager::sendSync(const juce::MidiMessage& msg)
{
    if (outputSyncDevices.size() > 0) {
        for (auto dev : outputSyncDevices)
          dev->sendMessageNow(msg);
    }
    else if (exportDevice) {
        exportDevice->sendMessageNow(msg);
    }
}

/**
 * Called through a LONG path from MidiTrack to convert the name
 * of a device from the Session into the internal id of the device.
 * This is just the index into the open devices array.
 * Returns -1 if not found.
 */
int MidiManager::getOutputDeviceId(const char* name)
{
    int id = -1;
    if (name != nullptr) {
        int index = 0;
        for (auto dev : outputDevices) {
            if (dev->getName() == name) {
                id = index;
                break;
            }
            index++;
        }
    }
    return id;
}

//////////////////////////////////////////////////////////////////////
//
// Device Information
//
// Methods here find information about the available devices and
// do not appear to need a functioning AudioDeviceManager.
// How MIDI device managemment interacts with AudioDevice manager
// is very unclear, but it seems to be an independent set of services,
// but you can get to them through AudioDeviceManager if you want?
//
// juce::MidiInput and juce::MidiOutput provide MidiDeviceInfo for
// the available devices.  Devices have a name which is human readable
// and an id which is not.  Docs say name "is provided by the OS unless
// the device has been created with the createNewDevice method.  Note that
// the name is not guaranteed to be unique".
//
// To open devices you need to do name to id mapping.  Since I only
// store names it is unclear under what conditions I would get non-unqiue
// names and what to do about them when it happens.  Unclear if ids
// survive across application executions or if they're generated at runtime.
// If they do survive execution, unclear if they are the same between
// different machines.
//
// Since I'm only prepared to save names, go with that and see what happens.
//
//////////////////////////////////////////////////////////////////////

/**
 * Return the list of available input devices.
 * Could be used to populate a selection menu.
 * Assuming that getAvailableDevices doesn't do a lot of work and there
 * isn't any advantage to saving the result for reuse.
 * 
 * Example does this:
 *  if (deviceManager.isMidiInputDeviceEnabled (input.identifier))
 * 
 * Find out the significance of being enabled and what it means
 * for the presentation of a selection list.  Why would you not just
 * show all of them and auto-enable it?
 */
juce::StringArray MidiManager::getInputDevices()
{
    juce::StringArray names;
    
    juce::Array<juce::MidiDeviceInfo> devices = juce::MidiInput::getAvailableDevices();
    for (auto info : devices) {
        names.add(info.name);
    }

    return names;
}

juce::StringArray MidiManager::getOutputDevices()
{
    juce::StringArray names;
    
    juce::Array<juce::MidiDeviceInfo> devices = juce::MidiOutput::getAvailableDevices();
    for (auto info : devices) {
        names.add(info.name);
    }

    return names;
}

/**
 * Map a device name to an id.
 */
juce::String MidiManager::getDeviceId(juce::Array<juce::MidiDeviceInfo> devices, juce::String name)
{
    juce::String id;
    for (auto info : devices) {
        if (info.name == name) {
            id = info.identifier;
            break;
        }
    }
    return id;
}

juce::String MidiManager::getInputDeviceId(juce::String name)
{
    return getDeviceId(juce::MidiInput::getAvailableDevices(), name);
}

juce::String MidiManager::getOutputDeviceId(juce::String name)
{
    return getDeviceId(juce::MidiOutput::getAvailableDevices(), name);
}

// this is needed only by MidiMonitorPanel 
juce::StringArray MidiManager::getOpenInputDevices()
{
    juce::StringArray names;

    for (auto dev : inputDevices)
      names.add(dev->getName());

    return names;
}

juce::StringArray MidiManager::getErrors()
{
    return errors;
}

//////////////////////////////////////////////////////////////////////
//
// MIDI Device Callbacks
//
// Whether opened direct or through ADM the callbacks should work the same.
// 
// These methods are called from the device handler thread
// NOT the application message thread so have to be careful.
//
// The examples used subclasses of juce::CallbackMessage to handle incomming
// messages which appears to be a way to pass a MidiMessage from the
// device thread to the UI message thread.  That's a good thing but
// it adds an underermined amount of overhead.  For everything but clocks
// it probably isn't significant.
//
// In my case I want MIDI events to pass through Binderator which will
// generate UIActions that are usually then queued for MobiusKernel
// in the audio thread.  There isn't much going on in that path except
// for the copying of the UIAction when crossing the shell/kernel boundary.
// It uses an action pool now so memory allocation is unlikely but possible.
//
// MIDI events can however be bound to actions that touch the UI.  Those mostly
// modify Component state and call repaint() so don't do anything too dangerous
// but in theory this could conflict with the maintenance thread.  Actions like
// reloading scripts or samples are much more problematic as they can take
// a long time.  So it is safest to use a CallbackMessage for everything
// to force processing on the UI thread.  The MidiMessage reference survives
// the transition because it is passed by value when the ListenerMessageCallback
// is created.
//
// When you get to the point where clock jitter is significant or there is noticeable
// lag betwen MIDI controller and UIAction handling consider a more direct path.
//
// The CallbackMessage subclasses were inner classes in the example, but I
// don't think that matters because the owner pointer (MainComponent) was passed
// in to the constructor and no parent class members were referenced i.e.
// it is not a closure.
//
//////////////////////////////////////////////////////////////////////

/**
 * This one is pure virtual and must be overridden
 * 
 * "A MidiInput object will call this method when a midi event arrives. It'll be called on
 * a high-priority system thread, so avoid doing anything time-consuming in here, and avoid
 * making any UI calls. You might find the MidiBuffer class helpful for queueing incoming
 * messages for use later."
 *
 * "The message's timestamp is set to a value equivalent to
 * (Time::getMillisecondCounter() / 1000.0) to specify the time when the message arrived"
 *
 * Jeff notes:
 * The example shows capturing the millisecond time when the app is started, so I guess
 * you could use that to calculate delta times here.
 *
 * Ahh, here is the mysterious "source" I've been wondering about.  This is how
 * the listener can know which MIDI device the message came from.  Not useful to me
 * yet, but I suppose you could have different binding assignments for different devices
 * someday.
 *
 */
void MidiManager::handleIncomingMidiMessage (juce::MidiInput* source,
                                             const juce::MidiMessage& message)
{
    // had to get this into an intermediate to avoid a "nonstandard extension" warning
    // passing source->getName() directly as a reference arg
    // !! there shouldn't be a need to pass the name around, tag them with an id
    // or something for internal use
    juce::String sourceName = source->getName();

    // handle realtime messages in this thread so they can be timestamp analyzed and
    // queued without delay
    const juce::uint8* raw = message.getRawData();
    const juce::uint8 status = *raw;

    if (status > 0xF0) {
        for (auto listener : realtimeListeners) {
            listener->midiRealtime(message, sourceName);
        }
    }
    else {
        postListenerMessage(message, sourceName);
    }

    // unclear whether sending MIDI is considered dangerous to do in the receiver thread,
    // let's try
    // might want some amount of filtering here, like supressing realtime
    // if there are several inputs configured, might want to designate only certain
    // ones to use thru
    // and beyond that this could get really flexible and allow each input to specific a
    // different thru device
    if (thruDevice != nullptr) {
        // since this is a reference, unclear if it would be modified
        // though I think the presence of "const" in the signature says it won't be
        thruDevice->sendMessageNow(message);
    }
}

/**
 * This one is optional, I don't need Sysex yet but I'm curious.
 *
 * "If a long sysex message is broken up into multiple packets, this callback is made
 * for each packet that arrives until the message is finished, at which point the
 * normal handleIncomingMidiMessage() callback will be made with the entire message.
 * The message passed in will contain the start of a sysex, but won't be finished
 * with the terminating 0xf7 byte."
 *
 * Interesting, why would you want to be informed of partial messages?
 */
void MidiManager::handlePartialSysexMessage(juce::MidiInput* source,
                                            const juce::uint8 *messageData,
                                            int numBytesSoFar,
                                            double timestamp)
{
    (void)source;
    (void)messageData;
    (void)numBytesSoFar;
    (void)timestamp;
    Trace(2, "MidiManager: Partial sysex received, why?\n");
}

/**
 * This is the CallbackMessage subclass that handles passing the MidiMessage
 * and MidiInput device name to the Juce message thread.  It is based on
 * an example I found somewhere.  I don't completely understand it but it works
 * well enough.
 *
 * postListenerMessage will create a new instance of this and then call the
 * post() method which puts it on a queue somewhere.  The use of "new" is
 * interesting here, I can't find the comments but I remember some discussion
 * of this being a reference counted pointer that will be deleted automatically
 * when the message is processed.  This will do memory allocation which is apparently
 * less of a no-no than it is in the audio thread.
 *
 * The class captures the owner pointer and any arguments we want to pass between
 * threads.  In the example, it did this for the owner:
 * ---
 * Component::SafePointer<MidiManager> owner;
 * 
 * Holds a pointer to some type of Component, which automatically becomes null if the
 * component is deleted.
 * If you're using a component which may be deleted by another event that's outside of
 * your control, use a SafePointer instead of a normal pointer to refer to it, and you
 * can test whether it's null before using it to see if something has deleted it.
 * 
 * The ComponentType template parameter must be Component, or some subclass of Component.
 * You may also want to use a WeakReference<Component> object for the same purpose.
 * ---
 *
 * In my case, MidiManager is not a Component so we couldn't use that anyway,
 * and it is a singleton managed by Supervisor and so will not be deleted before
 * the message queue goes away.
 *
 * I think...
 * Still need to work through destruction order between Supervisor and MainComponent
 * 
 */
class ListenerMessageCallback : public juce::CallbackMessage
{
  public:
    
    ListenerMessageCallback (MidiManager* o, const juce::MidiMessage& m, const juce::String& s)
        : owner (o), message (m), source (s)
    {}

    // this appears to be what Juce will call when it handles this
    // message in the event loop
    void messageCallback() override
    {
        if (owner != nullptr) {
            // and we can call any method on the owner
            owner->notifyListeners (message, source);
        }
    }

    // pointer back to us 
    MidiManager* owner;
    // these are copied by value so safe
    juce::MidiMessage message;
    juce::String source;
};

/**
 * Create an instance of ListenerMessageCallback and post it to the message queue.
 * Only need to bother with this if we have listeners.
 * It is allocated with new and will be magically freed when it gets processed
 * on the message thread.
 */
void MidiManager::postListenerMessage(const juce::MidiMessage& message, juce::String& source)
{
    if (listeners.size() > 0 || monitors.size() > 0) {
        (new ListenerMessageCallback (this, message, source))->post();
    }
}

/**
 * We're back from beyond and on the main event thread.
 * Could also retool this to transition the message to the maintenance thread
 * but then we can't use CallbackMessage.
 *
 * There are now two listeners, those for ordinary messages, and those for realtime
 * messages.
 *
 * Oh, and there is the "exclusive" listener that MidiPanel uses to do binding capture
 * without letting Binderator get in the way.  This does not include realtime messages.
 */
void MidiManager::notifyListeners(const juce::MidiMessage& message, juce::String& source)
{
    // Realtime messages start at 0xF8
    // System Common which includes SongPosition start at 0xF1
    // MidiRealizer wants SongPosition so pass everything in that range through
    // it will ignore most of them
    const juce::uint8* raw = message.getRawData();
    const juce::uint8 status = *raw;

    if (status > 0xF0) {
        // should have caught this above
    }
    else {
        bool processIt = true;

        for (auto monitor : monitors) {
            monitor->midiMonitor(message, source);
            if (monitor->midiMonitorExclusive())
              processIt = false;
        }
        
        if (processIt) {

            // now that we have MIDI tracks, there will be conflict over who gets
            // to own it, Binderator or MidiTracker
            // if binderator chooses to handle an event it shold probably not
            // be sent down
            // alternately, just send it to all the listeners and handle the ownership
            // at a higher level with channels or something
            for (auto listener : listeners)
              listener->midiMessage(message, source);

            if (recordable)
              record(message, source);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// MIDI Recording
//
//////////////////////////////////////////////////////////////////////

/**
 * If we decide something could be recorded, send it to the kernel.
 */
void MidiManager::record(const juce::MidiMessage& message, juce::String& source)
{
    MobiusInterface* mobius = supervisor->getMobius();
    if (mobius != nullptr) {
        int id = 0;
        int index = 0;
        for (auto dev : inputDevices) {
            if (dev->getName() == source) {
                id = index;
                break;
            }
            index++;
        }
        mobius->midiEvent(message, id);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Plugin MIDI Callback
//
//////////////////////////////////////////////////////////////////////

/**
 * Here through a very fragile path directly in the audio thread
 * when the plugin kernel receives MIDI from the host.
 *
 * If there are any monitors, save a copy of the message and
 * notify them later.
 *
 * I started this using a timer and a simple one-element "queue"
 * but it would be better if it were consistent with normal MidiInput
 * callbacks and used a CallbackMessage
 */
bool MidiManager::mobiusMidiReceived(juce::MidiMessage& message)
{
    bool passthrough = true;
    
    if (monitors.size() > 0) {

        // This is ugly, we can't give the monitors the message yet,
        // but we'd like to know if it wants to suppress further processing
        // or let it be handled normally.  MidiPanel and MidiDevicesPanel always
        // want exclusive access, but MidiMonitorPanel can be turned on an off.
        // Since any combination can be active at a time, assume if any one of them
        // is exclusive then pass through is disabled.
        for (auto monitor : monitors) {
            if (monitor->midiMonitorExclusive()) {
                passthrough = false;
                break;
            }
        }

        // save it in the "queue" for later
        pluginMessage = message;
        pluginMessageQueued = true;
    }

    return passthrough;
}

/**
 * Called periodically from the maintenance thread.
 * It is now safe for the monitors to display things.
 */
void MidiManager::performMaintenance()
{
    if (pluginMessageQueued) {
        juce::String source ("Host");

        for (auto monitor : monitors)
          monitor->midiMonitor(pluginMessage, source);

        // these are NEVER passed through to the normal listeners
        // kernel has it's own special Binderator for actions
        
        pluginMessageQueued = false;
    }
}

//////////////////////////////////////////////////////////////////////
//
// AudioDeviceManager Examples
//
// The code here is not used, and I'm not sure if it works but I think
// this is how you would use ADM to manage input devices should that
// ever become necessary.
//
//////////////////////////////////////////////////////////////////////
/*
 * Old notes from a tutorial:
 * ----
 * "Creates a default AudioDeviceManager"
 *
 * This class keeps tracks of a currently-selected audio device, through with which it
 * continuously streams data from an audio callback, as well as one or more midi inputs."
 * The idea is that your application will create one global instance of this object, and let
 * it take care of creating and deleting specific types of audio devices internally. So when
 * the device is changed, your callbacks will just keep running without having to worry
 * about this.
 *
 * To use an AudioDeviceManager, create one, and use initialise() to set it up.
 * Then call addAudioCallback() to register your audio callback with it, and use that to
 * process your audio data.
 *
 * The manager also acts as a handy hub for incoming midi messages, allowing a listener to
 * register for messages from either a specific midi device, or from whatever the current
 * default midi input device is. The listener then doesn't have to worry about re-registering
 * with different midi devices if they are changed or deleted.
 * 
 * And yet another neat trick is that amount of CPU time being used is measured and available
 * with the getCpuUsage() method.
 *
 * The AudioDeviceManager is a ChangeBroadcaster, and will send a change message to listeners
 * whenever one of its settings is changed.
 * ---
 * 
 * Okay, that partially explains the changing MIDI devices problem.  I guess it maintains
 * the listeners and will just stop calling them if it goes away.
 *
 * The MIDI example does not call initialize() so it seems necessary only if you
 * want audio, MIDI is ligher weight?
 *
 * MainComponent is an AudioAppComponent, the example isn't.
 * Yes, AudioAppComponent already has an AudioDeviceManager.  So we shouldn't make one here,
 * get it from our MainComponent
 *
 * This interesting for plugins...
 * It looks like you can build a private AudioDeviceManager just for MIDI without
 * asking it to do audio stuff.
 * 
 */

#if 0

/**
 * Open a MIDI input through the AudioDeviceManager.
 * Here you don't open/start/stop it, you "enable" it and
 * "add" a callback.
 *
 * Doc notes:
 * Enables or disables a midi input device.
 * 
 * The list of devices can be obtained with the MidiInput::getAvailableDevices() method.
 *
 * Any incoming messages from enabled input devices will be forwarded on to all the listeners
 * that have been registered with the addMidiInputDeviceCallback() method. They can either
 * register for messages from a particular device, or from just the "default" midi input.
 * 
 * Routing the midi input via an AudioDeviceManager means that when a listener registers
 * for the default midi input, this default device can be changed by the manager without
 * the listeners having to know about it or re-register.
 * 
 * It also means that a listener can stay registered for a midi input that is disabled
 * or not present, so that when the input is re-enabled, the listener will start receiving
 * messages again.
 *
 * Jeff notes:
 * That all seems well and good, but I'm not in a situation where the default
 * device is changing out from under me and devices aren't being stopped from elsewhere.
 * I suppose this makes it easier when managaging multiple devices.  You just enable
 * and register callbacks for any number of them, and ADM handles closing them when
 * it is destructed.
 */
void MidiManager::openInputADM(juce::String name)
{
    juce::String id = getInputDeviceId(name);
    if (id.isEmpty()) {
        Trace(1, "MidiManager: Unable to find id for MIDI input %s\n", name.toUTF8());
    }
    else if (lastInputInfo.identifier == id) {
        Trace(2, "MidiManager: Input device already open %s\n", name.toUTF8());
    }
    else {
        juce::AudioDeviceManager* deviceManager = supervisor->getAudioDeviceManager();
        if (deviceManager == nullptr) {
            Trace(1, "MidiManager: AudioDeviceManager not available, are you a plugin?\n");
        }
        else {
            closeInputADM();
            
            // auto-enable device
            // unclear under what circumstances devices become disabled
            // maybe they start that way?
            if (!deviceManager->isMidiInputDeviceEnabled(id))
              deviceManager->setMidiInputDeviceEnabled(id, true);

            // follow this device
            deviceManager->addMidiInputDeviceCallback (id, this);

            // don't seem to have an ADM interface for this
            lastInputInfo.name = name;
            lastInputInfo.identifier = id;

            DeviceConfig* config = supervisor->getDeviceConfig();
            MachineConfig* machine = config->getMachineConfig();
            machine->setMidiInput(name);
        }
    }
}

void MidiManager::closeInputADM()
{
    if (!lastInputInfo.identifier.isEmpty()) {
        juce::AudioDeviceManager* deviceManager = supervisor->getAudioDeviceManager();
        if (deviceManager == nullptr) {
            Trace(1, "MidiManager: AudioDeviceManager not available, are you a plugin?\n");
        }
        else {
            // examples did not show setting Enabled to false
            // it just took the callback away
            deviceManager->removeMidiInputDeviceCallback(lastInputInfo.identifier, this);
        }
        lastInputInfo.name = "";
        lastInputInfo.identifier = "";
    }
}

void MidiManager::openOutputADM(juce::String name)
{
    juce::String id = getOutputDeviceId(name);
    if (id.isEmpty()) {
        Trace(1, "MidiManager: Unable to find id for MIDI output %s\n", name.toUTF8());
    }
    else if (lastOutputInfo.identifier == id) {
        Trace(2, "MidiManager: Output device already open %s\n", name.toUTF8());
    }
    else {
        juce::AudioDeviceManager* deviceManager = supervisor->getAudioDeviceManager();
        if (deviceManager == nullptr) {
            Trace(1, "MidiManager: AudioDeviceManager not available, are you a plugin?\n");
        }
        else {
            closeOutputADM();

            // output devices don't behave like input devices
            // you don't enable them or add callbacks to them
            // all I see is setDefaultMidiOutputDevice
            // if you actually wanted to send something you would
            // have to call getDefaultMidiOutput to get a MidiOutput
            // is AudioDeviceManager really that useful?
            
            deviceManager->setDefaultMidiOutputDevice(id);

            // don't seem to have an ADM interface for this
            lastOutputInfo.name = name;
            lastOutputInfo.identifier = id;

            DeviceConfig* config = supervisor->getDeviceConfig();
            MachineConfig* machine = config->getMachineConfig();
            machine->setMidiInput(name);
        }
    }
}

/**
 * Unclear what we're supposed to do here.  All we did is call setDefaultMidiOutput
 * device.  There don't seem to be any resources we're supposed to reclaim.
 */
void MidiManager::closeOutputADM()
{
    if (!lastOutputInfo.identifier.isEmpty()) {
        lastOutputInfo.name = "";
        lastOutputInfo.identifier = "";
    }
}

#endif

//////////////////////////////////////////////////////////////////////
//
// Logging
//
// This was scraped from the HandlingMidiEvents tutorial.
// It was written to make use of a LogPanel and did a bunch of formatting
// that doesn't fit well with the Trace logger.
// Could rewrite someday, keep for reference.
//
//////////////////////////////////////////////////////////////////////

#if 0
/**
 * Source was the name of the MIDI input device
 *
 * getTimeStamp value depends on where the message came from.
 * When created by a MidiInput it will be:
 * "The message's timestamp is set to a value equivalent to
 * (Time::getMillisecondCounter() / 1000.0) to specify the time when
 * the message arrived.
 *
 */
void MidiManager::logMidiMessage(const juce::MidiMessage& message, juce::String& source)
{
    if (log != nullptr) {
        // showing time relative to when we were created
        auto time = message.getTimeStamp() - startTime;
        auto hours   = ((int) (time / 3600.0)) % 24;
        auto minutes = ((int) (time / 60.0)) % 60;
        auto seconds = ((int) time) % 60;
        auto millis  = ((int) (time * 1000.0)) % 1000;

        // render as a SMPTE-ish time code?
        auto timecode = juce::String::formatted ("%02d:%02d:%02d.%03d",
                                                 hours,
                                                 minutes,
                                                 seconds,
                                                 millis);

        auto description = getMidiMessageDescription (message);

        juce::String midiMessageString (timecode + "  -  " + description + " (" + source + ")");

        log->add(midiMessageString);
    }
}

/**
 * From the tutorial
 * This was static in the example, doesn't seem required 
 */
juce::String MidiManager::getMidiMessageDescription (const juce::MidiMessage& m)
{
    if (m.isNoteOn())           return "Note on "          + juce::MidiMessage::getMidiNoteName (m.getNoteNumber(), true, true, 3);
    if (m.isNoteOff())          return "Note off "         + juce::MidiMessage::getMidiNoteName (m.getNoteNumber(), true, true, 3);
    if (m.isProgramChange())    return "Program change "   + juce::String (m.getProgramChangeNumber());
    if (m.isPitchWheel())       return "Pitch wheel "      + juce::String (m.getPitchWheelValue());
    if (m.isAftertouch())       return "After touch "      + juce::MidiMessage::getMidiNoteName (m.getNoteNumber(), true, true, 3) +  ": " + juce::String (m.getAfterTouchValue());
    if (m.isChannelPressure())  return "Channel pressure " + juce::String (m.getChannelPressureValue());
    if (m.isAllNotesOff())      return "All notes off";
    if (m.isAllSoundOff())      return "All sound off";
    if (m.isMetaEvent())        return "Meta event";

    if (m.isController())
    {
        juce::String name (juce::MidiMessage::getControllerName (m.getControllerNumber()));

        if (name.isEmpty())
          name = "[" + juce::String (m.getControllerNumber()) + "]";

        return "Controller " + name + ": " + juce::String (m.getControllerValue());
    }

    return juce::String::toHexString (m.getRawData(), m.getRawDataSize());
}
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
