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

#include "MidiWatcher.h"
#include "MidiHarvester.h"

class MidiRecorder : public MidiWatcher::Listener
{
  public:

    //
    // Configuration
    //
    
    MidiRecorder(class MidiTrack* t);
    ~MidiRecorder();
    void initialize(class MidiLayerPool* lpool,
                    class MidiSequencePool* spool,
                    class MidiEventPool* epool,
                    class MidiSegmentPool* segpool);

    void dump(class StructureDumper& d);
    
    //
    // Transaction Management
    //
    
    void reset();
    void setFrames(int frames);
    void setCycles(int cycles);
    void begin();
    void resume(MidiLayer* layer);
    void rollback(bool overdub);
    void clear();
    MidiLayer* commit(bool overdub);
    MidiLayer* commitMultiply(bool overdub, bool unrounded);
    void setFrame(int newFrame);

    void startMultiply();
    void endMultiply(bool overdub);
    int getMultiplyFrame();
    
    void startInsert();
    void endInsert(bool overdub);

    void copy(MidiLayer* srcLayer, bool includeEvents);

    void startReplace();
    void endReplace(bool overdub);
    
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
    // MIDI events
    //

    void noteOn(class MidiEvent* e);
    void noteOff(class MidiEvent* e);
    void midiEvent(class MidiEvent* e);

    //
    // Edits
    //

    bool isRecording();
    void setRecording(bool b);
    bool isExtending();
    void setExtending(bool b);
    void advance(int frames);
    void add(class MidiEvent* e);

    void watchedNoteOn(class MidiEvent* e) override;
    void watchedNoteOff(class MidiEvent* on, class MidiEvent* off) override;
    void watchedEvent(class MidiEvent* e) override;
    
  private:

    class MidiTrack* track = nullptr;
    
    // provided resources
    class MidiLayerPool* layerPool = nullptr;
    class MidiSequencePool* sequencePool = nullptr;
    class MidiEventPool* midiPool = nullptr;
    class MidiSegmentPool* segmentPool = nullptr;

    // held note monitor
    MidiWatcher watcher;
    // segment prefix analyzer
    MidiHarvester harvester;
    
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
    bool multiply = false;
    int multiplyFrame = 0;
    int extensions = 0;
    bool replace = false;

    int lastBlockFrames = 0;

    void extend();
    MidiSegment* rebuildSegments(int startFrame,  int endFame);
    
    void assimilate(class MidiLayer* layer);
    class MidiLayer* prepLayer();
    
    class MidiEvent* copyEvent(class MidiEvent* src);

    void injectHeld();
    void finalizeHeld();
    void finalizeHold(class MidiEvent* note, class MidiEvent* off);
    
};
