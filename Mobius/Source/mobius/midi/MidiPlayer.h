/**
 * Helper class for MidiTrack to manage state related to playing.
 */

#pragma once

#include <JuceHeader.h>

class MidiPlayer
{
  public:
    
    MidiPlayer(class MidiTrack* t);
    ~MidiPlayer();
    void initialize(class MobiusContainer* c, class MidiNotePool* pool);
    void setDurationMode(bool durationMode);
    
    // initialize state for playing the track
    void reset();
    void setLayer(class MidiLayer* l);
    void setFrame(int frame);
    void shift(class MidiLayer* l);
    void restart();
    
    void play(int frames);
    int getFrame();
    int getFrames();
    int getCycles();

  private:

    class MobiusContainer* container = nullptr;
    class MidiNotePool* notePool = nullptr;
    class MidiTrack* track = nullptr;
    class MidiLayer* layer = nullptr;

    class MidiNote* heldNotes = nullptr;
    bool durationMode = false;
    
    int playFrame = 0;
    int loopFrames = 0;
    int cycles = 0;

    juce::Array<class MidiEvent*> currentEvents;
    juce::Array<class MidiEvent*> notesOn;
    
    void send(class MidiEvent* e);
    void flushHeld();
    void advanceHeld(int blockFrames);
    void forceHeld();
    void sendOff(class MidiNote* note);

    // tracking when durationMode is off
    void trackNoteOn(class MidiEvent* e);
    void trackNoteOff(class MidiEvent* e);
    void alloff();

    const int MaxNotes = 64;

    
};


