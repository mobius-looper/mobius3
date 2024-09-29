/**
 * Manages the MIDI recording process for one track.
 */

#pragma once

#include <JuceHeader.h>

class MidiRecorder
{
  public:

    MidiRecorder(class MidiTrack* track);
    ~MidiRecorder();
    void initialize(class MidiLayerPool* lpool, class MidiSequencePool* spool,
                    class MidiEventPool* epool, class MidiSegmentPool* segpool,
                    class MidiNotePool* npool);


    void reset();
    bool hasChanges();
    int getEventCount();
    void advance(int frames);
    void add(class MidiEvent* e);
    class MidiLayer* commit(int frames, bool continueHolding);
    void finalizeHeld();

    // test hack
    void setDurationMode(bool durationMode);

  private:

    class MidiTrack* track = nullptr;
    class MidiLayerPool* layerPool = nullptr;
    class MidiSequencePool* sequencePool = nullptr;
    class MidiEventPool* midiPool = nullptr;
    class MidiSegmentPool* segmentPool = nullptr;
    class MidiNotePool* notePool = nullptr;
    
    class MidiLayer* recordLayer = nullptr;
    class MidiNote* heldNotes = nullptr;
    int lastBlockFrames = 0;
    bool durationMode = false;
    
    class MidiLayer* prepLayer();
    void flushHeld();
    class MidiNote* removeNote(class MidiEvent* e);
    void finalizeHold(class MidiNote* note, class MidiEvent* e);
    
};
