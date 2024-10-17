/**
 * Manages the MIDI playback process for a MidiTrack
 */

#pragma once

#include <JuceHeader.h>

#include "MidiHarvester.h"

class MidiPlayer
{
  public:

    //
    // Configuration
    //
    MidiPlayer(class MidiTrack* t);
    ~MidiPlayer();
    void initialize(class MobiusContainer* c, class MidiPools* pools);
    void dump(StructureDumper& d);
    
    //
    // Layer Management
    //

    void reset();
    void change(class MidiLayer* l, int newFrame = -1);
    void setFrame(int frame);
    void restart();
    void shift(class MidiLayer* l);
    void setMute(bool b);
    bool isMuted();
    
    //
    // Play State
    //
    
    int getFrame();
    int getFrames();

    //
    // Play Advance
    //

    void play(int frames);

    // need this to send on all channels all devices
    //void allNotesOff();

    // store a playback checkpoint at the current frame
    void checkpoint();

    // pause playback
    void pause();
    void unpause(bool noHold=false);
    bool isPaused();
    
  private:

    // configuration
    class MobiusContainer* container = nullptr;
    class MidiPools* pools = nullptr;
    
    class MidiTrack* track = nullptr;

    // play state
    class MidiLayer* playLayer = nullptr;
    int playFrame = 0;
    int loopFrames = 0;
    bool muted = false;
    bool paused = false;
    class MidiFragment* restoredHeld = nullptr;
    
    // transient buffers used during event gathering
    MidiHarvester harvester;

    // note duration tracking state
    class MidiEvent* heldNotes = nullptr;
    
    void play(class MidiEvent* n);
    void setMuteInternal(bool b, bool setMuteMode);
    void sendOn(class MidiEvent* e);
    void flushHeld();
    void advanceHeld(int blockFrames);
    void forceOff();
    void sendOff(class MidiEvent* note);
    void saveHeld();
    void prepareHeld();
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

