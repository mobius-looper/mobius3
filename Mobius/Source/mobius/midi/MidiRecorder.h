/**
 * Manages the MIDI recording process for one track.
 *
 * The recorder is responsible for accumulating midi events that
 * come into a track and managing the adjustment to baking segments
 * as they become occluded by a new overdubs.
 *
 */

#pragma once

#include <JuceHeader.h>

class MidiRecorder
{
  public:

    //
    // Configuration
    //
    
    MidiRecorder();
    ~MidiRecorder();
    void initialize(class MidiLayerPool* lpool,
                    class MidiSequencePool* spool,
                    class MidiEventPool* epool,
                    class MidiSegmentPool* segpool,
                    class MidiNotePool* npool);
    
    // test hack
    void setDurationMode(bool durationMode);

    //
    // Transaction Management
    //
    
    void reset();
    void begin();
    void resume(MidiLayer* layer);
    void rollback();
    void clear();
    MidiLayer* commit(bool continueHolding);
    void setFrame(int newFrame);

    //
    // Transaction State
    //

    int getFrames();
    int getFrame();
    int getCycles();
    int getCycleFrames();
    bool hasChanges();
    int getEventCount();

    //
    // Edits
    //

    bool isRecording();
    void setRecording(bool b);
    bool isExtending();
    void setExtending(bool b);
    void advance(int frames);
    void add(class MidiEvent* e);

  private:

    // provided resources
    class MidiLayerPool* layerPool = nullptr;
    class MidiSequencePool* sequencePool = nullptr;
    class MidiEventPool* midiPool = nullptr;
    class MidiSegmentPool* segmentPool = nullptr;
    class MidiNotePool* notePool = nullptr;

    // configuration options
    bool durationMode = false;
    
    // the backing layer for the transaction
    class MidiLayer* backingLayer = nullptr;

    // the transaction in progress
    class MidiLayer* recordLayer = nullptr;
    int recordFrames = 0;
    int recordFrame = 0;
    int recordCycles = 1;
    int cycleFrames = 0;
    bool recording = false;
    bool extending = false;
    int extensions = 0;
    
    // held note tracking
    class MidiNote* heldNotes = nullptr;
    int lastBlockFrames = 0;

    void assimilate(class MidiLayer* layer);
    class MidiLayer* prepLayer();
    
    void flushHeld();
    void advanceHeld(int blockFrames);
    class MidiNote* removeHeld(class MidiEvent* e);
    void finalizeHeld();
    void finalizeHold(class MidiNote* note, class MidiEvent* e);
    
};
