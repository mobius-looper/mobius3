/**
 * Manages the MIDI playback process for a MidiTrack
 */

#pragma once

#include <JuceHeader.h>

class MidiPlayer
{
  public:

    //
    // Configuration
    //
    MidiPlayer();
    ~MidiPlayer();
    void initialize(class MobiusContainer* c, class MidiNotePool* pool);
    void setDurationMode(bool durationMode);

    //
    // Layer Management
    //

    void reset();
    void setLayer(class MidiLayer* l);
    void setFrame(int frame);
    void restart();
    void shift(class MidiLayer* l);

    //
    // Play State
    //
    
    int getFrame();
    int getFrames();

    //
    // Play Advance
    //

    void alloff();
    void play(int frames);

  private:

    // configuration
    class MobiusContainer* container = nullptr;
    class MidiNotePool* notePool = nullptr;
    bool durationMode = false;

    // play state
    class MidiLayer* playLayer = nullptr;
    int playFrame = 0;
    int loopFrames = 0;

    // transient buffer used during event gathering
    juce::Array<class MidiEvent*> currentEvents;

    // new duration tracking state
    class MidiNote* heldNotes = nullptr;
    
    // old on/off tracking state
    juce::Array<class MidiEvent*> notesOn;
    
    void send(class MidiEvent* e);

    // duration based tracking
    void flushHeld();
    void advanceHeld(int blockFrames);
    void forceHeld();
    void sendOff(class MidiNote* note);

    // tracking when durationMode is off
    const int MaxNotes = 64;
    void trackNoteOn(class MidiEvent* e);
    void trackNoteOff(class MidiEvent* e);
    void allNotesOff();
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

