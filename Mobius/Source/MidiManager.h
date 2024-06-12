/**
 * Code related to the handling of MIDI devices.
 * This is effectively a singleton class owned by Supervisor.
 *
 * This will handle opening and closing of the configured devices
 * and receive the event callbacks.  Events received through the
 * callback are forward to one or more listeners.
 *
 * It is primarily used when running as a standalone application,
 * but can be used in a very limited way by a plugin to send MIDI clocks
 * to a single private output device outside the host application's control.
 * It is unclear after all these years if this still needs to be done,
 * I did it originally to reduce clock jitter but these days things may be
 * fast enough just to pass clocks through the usual host MIDI streams.
 *
 * Only supporting a single input and output device right now which
 * is enough for testing and most people don't use the standalone app.
 */

#pragma once
#include <JuceHeader.h>

class MidiManager : public juce::MidiInputCallback
{
  public:

    /**
     * Interface of an object that wants to receive MidiMessages.
     * Used by Binderator and MidiDevicePanel.
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

    MidiManager(class Supervisor* super);
    ~MidiManager();

    void addListener(Listener* l);
    void removeListener(Listener* l);
    void setExclusiveListener(Listener* l);
    void removeExclusiveListener();
    
    void addRealtimeListener(RealtimeListener* l);
    void removeRealtimeListener(RealtimeListener* l);

    void openDevices();
    void suspend();
    void resume();
    
    // Close devices and remove callbacks
    void shutdown();

    bool hasOutputDevice();
    void send(juce::MidiMessage msg);
    void sendSync(juce::MidiMessage msg);

    // Device Information
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

    void mobiusMidiReceived(juce::MidiMessage& msg);
    
  private:


    class Supervisor* supervisor = nullptr;

    class juce::Array<Listener*> listeners;
    Listener* exclusiveListener = nullptr;
    class juce::Array<RealtimeListener*> realtimeListeners;

    // this isn't the way you're supposed to do it, but I fucking
    // hate dealing with unique_ptr, I don't need my goddam dick held for me
    //juce::Array<std::unique_ptr<juce::MidiInput>> inputDevices;
    juce::OwnedArray<juce::MidiInput> inputDevices;
    std::unique_ptr<juce::MidiOutput> outputDevice = nullptr;
    std::unique_ptr<juce::MidiOutput> outputSyncDevice = nullptr;
    
    // tutorial captures this on creation to show relative times
    // when logging incomming MIDI messages
    double startTime;
    
    void configurePluginListening();
    juce::String getDeviceId(juce::Array<juce::MidiDeviceInfo> devices, juce::String name);
    juce::String getInputDeviceId(juce::String name);
    juce::String getOutputDeviceId(juce::String name);

    juce::String getFirstName(juce::String csv);
    void stopInputs();
    void closeInputs();
    void openInputs(juce::String csv);
    void reopenInputs();
    std::unique_ptr<juce::MidiInput> MidiManager::openNewInput(juce::String name);
    void openOutput(juce::String name, bool sync);

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
    
    
