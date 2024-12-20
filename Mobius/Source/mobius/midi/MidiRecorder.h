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
    void initialize(class MidiPools* p);
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
    MidiLayer* commitMultiply(bool overdub, bool unrounded);
    void setFrame(int newFrame);

    // called when the layer needs to be shifted
    // handles all modes and is the only way to do unrounded endings
    MidiLayer* commit(bool overdub, bool unrounded);
    
    int getModeStartFrame();
    int getModeEndFrame();

    // replace may start/end without a full commit
    void startReplace();
    void finishReplace(bool overdub);
    
    // there is no finishMultiply, you must call commit()
    void startMultiply();
    void extendMultiply();
    void reduceMultiply();

    // insert may start/end without a full commit if rounding
    // for unrounded you must call commit
    void startInsert();
    void extendInsert();
    void reduceInsert();
    void finishInsert(bool overdub);

    void copy(MidiLayer* srcLayer, bool includeEvents);

    bool isEmpty();
    bool isInstantClean();
    void instantMultiply(int n);
    void instantDivide(int n);

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
    int captureEventsReceived();
    
    //
    // Edits
    //

    bool isRecording();
    void setRecording(bool b);
    bool isExtending();
    void setExtending(bool b);
    bool isReplace();
    void advance(int frames);
    void add(class MidiEvent* e);

    void watchedNoteOn(class MidiEvent* e) override;
    void watchedNoteOff(class MidiEvent* on, class MidiEvent* off) override;
    void watchedEvent(class MidiEvent* e) override;
    
  private:

    class MidiTrack* track = nullptr;
    
    // provided resources
    class MidiPools* pools = nullptr;

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
    int extensions = 0;
    int modeStartFrame = 0;
    int modeEndFrame = 0;
    bool replace = false;
    bool multiply = false;
    bool insert = false;

    int lastBlockFrames = 0;
    int eventsReceived = 0;

    void resetFlags();
    void addMultiplyCycle();
    void finishMultiply(bool unrounded);
    MidiSegment* rebuildSegments(int startFrame,  int endFame);
    void finishInsertInternal(bool unrounded);

    void appendCycle(class MidiSegment* src, int cycle);
    void truncateCycle();
    
    void assimilate(class MidiLayer* layer);
    class MidiLayer* prepLayer();
    
    class MidiEvent* copyEvent(class MidiEvent* src);

    void injectHeld();
    void finalizeHeld();
    void finalizeHold(class MidiEvent* note, class MidiEvent* off);
    
};
