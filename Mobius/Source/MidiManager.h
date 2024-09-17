/**
 * Code related to the handling of MIDI devices.
 * This is effectively a singleton class owned by Supervisor.
 *
 * This will handle opening and closing of the configured devices
 * and receive the event callbacks.  Events received through the
 * callback are forward to one or more listeners.
 *
 * It is primarily used when running as a standalone application,
 * but can be used in a more limited way by a plugin to do MIDI things when
 * inside hosts that don't provide enough flexibility.  A notable use is
 * when sending low-jitter MIDI clocks to an external device.
 *
 * There are two types of listeners that can be installed:
 * 
 *   processing listeners
 *   monitoring listeners
 *
 * A processing listener handles events received directly from a MidiInput
 * and normally maps those to actions.
 *
 * A monitoring listener is used to display information about the MIDI being
 * received but does not always want them to be processed.
 *
 * Monitoring listeners can receive events both from directly opened devices,
 * and indirectly from the kernel when it receives events through the plugin host.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "model/DeviceConfig.h"
#include "mobius/MobiusInterface.h"

class MidiManager : public juce::MidiInputCallback, public MobiusMidiListener
{
  public:

    /**
     * Internal usage indiciators for devices tagged for some special purpose.
     */
    enum Usage {
        Input,
        InputSync,
        Output,
        Export,
        OutputSync,
        Thru
    };

    /**
     * Interface of an object that wants to receive MidiMessages for processing.
     * Only normal consumer is Binderator.
     * This is used only for non-realtime messages like notes, controllers, etc.
     */
    class Listener {
      public:
        virtual ~Listener() {}
        virtual void midiMessage(const juce::MidiMessage& message, juce::String& source) = 0;
    };

    /**
     * Interface of an object that wants to receive realtime messages
     * like Start, Stop, Continue, Clock.
     * The main consumer is MidiRealizer
     */
    class RealtimeListener {
      public:
        virtual ~RealtimeListener() {}
        virtual void midiRealtime(const juce::MidiMessage& message, juce::String& source) = 0;
    };

    /**
     * Interface of an object that wants to receive MidiMessages for monitoring.
     * Used by MidiMonitorPanel, MidiPanel, and MidiDevicesPanel.
     */
    class Monitor {
      public:
        virtual ~Monitor() {}
        virtual void midiMonitor(const juce::MidiMessage& message, juce::String& source) = 0;
        virtual bool midiMonitorExclusive() = 0;
        virtual void midiMonitorMessage(juce::String msg) {}
    };
    
    MidiManager(class Supervisor* super);
    ~MidiManager();

    void addListener(Listener* l);
    void removeListener(Listener* l);

    void addRealtimeListener(RealtimeListener* l);
    void removeRealtimeListener(RealtimeListener* l);
    
    void addMonitor(Monitor* l);
    void removeMonitor(Monitor* l);

    // open all configured devices at startup or when
    // doing bulk configuration changes
    void openDevices();

    // open and close devices individually
    // intended for use by the MidiDeviceEditor as selections are made
    void openInput(juce::String name, Usage usage);
    void closeInput(juce::String name,Usage usage);
    void openOutput(juce::String name, Usage usage);
    void closeOutput(juce::String name, Usage usage);

    // temporarily disable inputs
    void suspend();
    void resume();

    // process queued monitoring events from the audio thread
    void performMaintenance();

    // current device status
    juce::StringArray getErrors();
    juce::StringArray getOpenInputDevices();
    juce::StringArray getOpenOutputDevices();
    
    // Close devices and remove callbacks
    void shutdown();

    bool hasOutputDevice(Usage usage);

    // used for MIDI state export and the MidiOut script statement
    // uses the device configured for Export usage
    void send(juce::MidiMessage msg);
    
    // used for MIDI clocks, uses the device configured for OutputSync usage
    void sendSync(juce::MidiMessage msg);

    // Available device information
    juce::StringArray getInputDevices();
    juce::StringArray getOutputDevices();

    // MidiInputCallback

    // Example has these as private and the inheritance as private
    // which makes sense, but I don't understand how C++ allows access
    void handleIncomingMidiMessage (juce::MidiInput* source,
                                    const juce::MidiMessage& message) override;

    void handlePartialSysexMessage(juce::MidiInput* source,
                                   const juce::uint8 *messageData,
                                   int numBytesSoFar,
                                   double timestamp) override;

    // needs to be public so it can be called from a CallbackMessage
    void notifyListeners(const juce::MidiMessage& message, juce::String& source);

    // called in the audio thread as events are received by the plugin
    bool mobiusMidiReceived(juce::MidiMessage& msg) override;
    
  private:

    class Supervisor* supervisor = nullptr;

    class juce::Array<Listener*> listeners;
    class juce::Array<RealtimeListener*> realtimeListeners;
    class juce::Array<Monitor*> monitors;

    // error messages from the last time openDevices was called
    juce::StringArray errors;

    // device names captured from the MachineConfig and used to
    // open devices, these are authorative over which devices are open
    juce::StringArray inputNames;
    juce::StringArray outputNames;

    // pointers to open devices
    // these may be out of sync with the name arrays and will
    // be brought back into sync by opening and closing devices
    juce::OwnedArray<juce::MidiInput> inputDevices;
    juce::OwnedArray<juce::MidiOutput> outputDevices;

    // usage pointers for devices in one of the two device lists
    juce::MidiInput* inputSyncDevice = nullptr;
    juce::MidiOutput* exportDevice = nullptr;
    juce::MidiOutput* outputSyncDevice = nullptr;
    juce::MidiOutput* thruDevice = nullptr;
    
    // tutorial captures this on creation to show relative times
    // when logging incomming MIDI messages
    double startTime;

    // holding area for messages received from the plugin audio thread
    juce::MidiMessage pluginMessage;
    bool pluginMessageQueued = false;
    
    void somethingBadHappened(juce::String msg);
    void monitorMessage(juce::String msg);

    juce::String getDeviceId(juce::Array<juce::MidiDeviceInfo> devices, juce::String name);
    juce::String getInputDeviceId(juce::String name);
    juce::String getOutputDeviceId(juce::String name);

    juce::String getDeviceName(class MachineConfig* config, Usage usage);
    juce::String getFirstName(juce::String csv, Usage usage);
    juce::String getUsageName(Usage usage);

    void reconcileInputs(MachineConfig* config);
    juce::MidiInput* findInput(juce::String name);
    juce::MidiInput* findOrOpenInput(juce::String name);
    void closeUnusedInputs();
    void stopInputs();
    void startInputs();
    void closeAllInputs();

    void reconcileOutputs(MachineConfig* config);
    juce::MidiOutput* findOutput(juce::String name);
    juce::MidiOutput* findOrOpenOutput(juce::String name);
    void openOutputInternal(juce::String name, Usage usage);
    void closeUnusedOutputs();
    void closeAllOutputs();

    void postListenerMessage (const juce::MidiMessage& message, juce::String& source);

    // experiments
#if 0    
    // Device identifiiers when opened through AudioDeviceManager
    juce::MidiDeviceInfo lastInputInfo;
    juce::MidiDeviceInfo lastOutputInfo;

    void openInputADM(juce::String name);
    void closeInputADM();
    void openOutputADM(juce::String name);
    void closeOutputADM();
#endif
    
    // leftovers from the tutorial
    // static juce::String getMidiMessageDescription (const class juce::MidiMessage& m);

};
    
    
