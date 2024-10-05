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
                    class MidiSegmentPool* segpool,
                    class MidiNotePool* npool);
    
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
    void setFrame(int newFrame);

    void startMultiply();
    void endMultiply(bool overdub, bool unrounded);
    
    void startInsert();
    void endInsert(bool overdub);

    void copy(MidiLayer* srcLayer, bool includeEvents);
    
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

    void noteOn(class MidiEvent* e, MidiNote* n);
    void noteOff(class MidiEvent* e, MidiNote* n);
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

    void watchedNoteOn(class MidiEvent* e, class MidiNote* n) override;
    void watchedNoteOff(class MidiEvent* e, class MidiNote* n) override;
    void watchedEvent(class MidiEvent* e) override;
    
  private:

    class MidiTrack* track = nullptr;
    
    // provided resources
    class MidiLayerPool* layerPool = nullptr;
    class MidiSequencePool* sequencePool = nullptr;
    class MidiEventPool* midiPool = nullptr;
    class MidiSegmentPool* segmentPool = nullptr;
    class MidiNotePool* notePool = nullptr;

    // held note monitor
    MidiWatcher watcher;
    
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

    int lastBlockFrames = 0;

    void assimilate(class MidiLayer* layer);
    class MidiLayer* prepLayer();
    
    class MidiEvent* copyEvent(class MidiEvent* src);
    class MidiNote* copyNote(class MidiNote* src);

    void injectHeld();
    void finalizeHeld();
    void finalizeHold(class MidiNote* note, class MidiEvent* e);
    
};
