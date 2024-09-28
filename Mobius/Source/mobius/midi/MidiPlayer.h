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
    void initialize(class MobiusContainer* c);
    
    // initialize state for playing the track
    void reset();
    void setLayer(class MidiLayer* l);
    void play(int frames);

  private:

    class MobiusContainer* container = nullptr;
    class MidiTrack* track = nullptr;
    class MidiLayer* layer = nullptr;

    int playFrame = 0;
    int loopFrames = 0;

    juce::Array<class MidiEvent*> currentEvents;
    juce::Array<class MidiEvent*> notesOn;
    
    void send(MidiEvent* e);
    void trackNoteOn(class MidiEvent* e);
    void trackNoteOff(class MidiEvent* e);
    void alloff();

    const int MaxNotes = 64;

    
};


