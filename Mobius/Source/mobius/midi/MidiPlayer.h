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
    void initialize(class MidiPools* pools);
    void dump(StructureDumper& d);
    
    //
    // Layer Management
    //

    void reset();
    void change(class MidiLayer* l, int newFrame = -1);
    void shift(class MidiLayer* l);
    void restart();
    
    //
    // Play State
    //
    
    int getFrame();
    int getFrames();
    int captureEventsSent();
    void setMute(bool b);
    bool isMuted();
    void setPause(bool b, bool noHold = false);
    bool isPaused();
    
    void stop();
    void setFrame(int frame);
    
    //
    // Play Advance
    //

    void play(int frames);

    // need this to send on all channels all devices
    //void allNotesOff();

    // store a playback checkpoint at the current frame
    void checkpoint();

    void setDeviceId(int id);
    int getDeviceId();
    
  private:

    // configuration
    class MobiusContainer* container = nullptr;
    class MidiPools* pools = nullptr;
    
    class MidiTrack* track = nullptr;

    // the id of the device we're supposed to send to
    int outputDevice = 0;

    // play state
    class MidiLayer* playLayer = nullptr;
    int playFrame = 0;
    int loopFrames = 0;
    bool muted = false;
    bool paused = false;
    class MidiFragment* restoredHeld = nullptr;
    int eventsSent = 0;
    
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

