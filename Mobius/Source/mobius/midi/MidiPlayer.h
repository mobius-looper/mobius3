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
    void initialize(class MobiusContainer* c, class MidiNotePool* pool);

    //
    // Layer Management
    //

    void reset();
    void setLayer(class MidiLayer* l);
    void setFrame(int frame);
    void restart();
    void shift(class MidiLayer* l);
    void setMute(bool b);
    bool isMute();
    
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
    
  private:

    // configuration
    class MobiusContainer* container = nullptr;
    class MidiNotePool* notePool = nullptr;
    class MidiTrack* track = nullptr;

    // play state
    class MidiLayer* playLayer = nullptr;
    int playFrame = 0;
    int loopFrames = 0;
    bool mute = false;
    
    // transient buffers used during event gathering
    MidiHarvester harvester;

    // note duration tracking state
    class MidiNote* heldNotes = nullptr;
    
    void play(class MidiNote* n);
    void sendOn(class MidiNote* e);
    void flushHeld();
    void advanceHeld(int blockFrames);
    void forceOff();
    void sendOff(class MidiNote* note);
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

