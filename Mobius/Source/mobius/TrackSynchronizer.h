/**
 * New refactoring of core/Synchroizer that focuses just on merging the
 * various sync sources into a uniform set of sync events for each audio block.
 *
 * Not specifically related to any "track" concept, consider generalizing the names
 * and interfaces.  What tracks really are are another form of sync source.
 */

#pragma once

#include <JuceHeader.h>

class TrackSynchronizer
{
  public:

    typedef enum {
        SourceMidiIn,
        SourceMidiOut,
        SourceHost,
        SourceInternal
    } Source;

    typedef enum {
        TypeStop,
        TypeStart,
        TypeContinue,
        TypePulse
    } Type;

    typedef enum {
        PulseNone,
        PulseClock,
        PulseBeat,
        PulseBar
    } PulseType;

    /**
     * Events we manage, consolodated from the various sources.
     * As this firms up, consider having MidiRealizer share the same
     * event model?
     */
    class Event {
      public:
        Event() {}
        ~Event() {}

        // chain and pool pointer
        Event* next = nullptr;
        int millisecond = 0;
        Source source = SourceInternal;
        Type type = TypePulse;
        PulseType pulse = PulseClock;
        // midi pulse info
        int continuePulse = 0;
        int beat = 0;
        // host pulse info
        int frame = 0;
    };

    TrackSynchronizer(class MobiusKernel* k);
    ~TrackSynchronizer();

    void initialize();
    void interruptStart(class MobiusAudioStream* stream);
    
  private:

    class MobiusKernel* kernel = nullptr;
    class MobiusMidiTransport* midiTransport = nullptr;

    // random statistics
    int lastInterruptMsec = 0;
    int interruptMsec = 0;
    int interruptFrames = 0;
    
    // the events we manage
    Event* events = nullptr;
    Event* eventPool = nullptr;

    // host sync state
    float hostTempo = 0.0f;
    int hostBeat = 0;
    int hostBar = 0;
    int hostBeatsPerBar = 0;
    bool hostTransport = false;
    bool hostTransportPending = false;
    
    void flushEvents();
    Event* newEvent();
    void freeEvent(Event* event);
    void gatherMidi();
    int getMidiInBeatsPerBar();
    int getMidiOutBeatsPerBar();

    void gatherHost(class MobiusAudioStream* stream);
            
    Event* convertEvent(class MidiSyncEvent* mse, int beatsPerBar);

};
