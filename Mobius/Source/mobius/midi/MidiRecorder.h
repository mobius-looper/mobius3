/**
 * Manages the MIDI recording process for one track.
 *
 * The recorder is responsible for accumulating midi events that
 * come into a track and managing the adjustment to baking segments
 * as they become occluded by a new overdubs.
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


    // release all state held by the recorder and reset the location
    void reset();

    // release resources held by recorder but keep the location
    void clear();
    
    bool isRecording();
    void setRecording(bool b);
    bool isExtending();
    void setExtending(bool b);
    
    int getFrames();
    int getFrame();
    void setFrame(int newFrame);
    int getCycles();
    int getCycleFrames();
    bool hasChanges();
    int getEventCount();
    
    void advanceHeld(int blockFrames);
    void advance(int frames);
    void add(class MidiEvent* e);
    class MidiLayer* commit(bool continueHolding);
    void finalizeHeld();
    void resume(MidiLayer* layer);

    // test hack
    void setDurationMode(bool durationMode);

  private:

    class MidiTrack* track = nullptr;
    class MidiLayerPool* layerPool = nullptr;
    class MidiSequencePool* sequencePool = nullptr;
    class MidiEventPool* midiPool = nullptr;
    class MidiSegmentPool* segmentPool = nullptr;
    class MidiNotePool* notePool = nullptr;

    bool recording = false;
    bool extending = false;
    int frame = 0;
    int frames = 0;
    int cycles = 1;
    int cycleFrames = 0;
    
    class MidiLayer* recordLayer = nullptr;
    class MidiNote* heldNotes = nullptr;
    int lastBlockFrames = 0;
    bool durationMode = false;
    
    class MidiLayer* prepLayer();
    void flushHeld();
    class MidiNote* removeNote(class MidiEvent* e);
    void finalizeHold(class MidiNote* note, class MidiEvent* e);
    
};
