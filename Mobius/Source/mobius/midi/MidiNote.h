/**
 * Used by MidiPlayer to hold notes returned by the layers, sustain
 * them for a period of time, and cancel them when layers change.
 *
 * Numbers related to durations are in units of audio frames.
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/ObjectPool.h"

class MidiNote : public PooledObject
{
  public:

    MidiNote();
    ~MidiNote();
    void poolInit();

    // player chain pointer
    MidiNote* next = nullptr;

    // midi channel number
    // uses Juce convention of 1 based for specific channels
    // 0 means "unspecified" and might be set for host MIDI events
    int channel = 0;

    // midi note number
    int number = 0;

    // release velocity
    // not implemented yet, and when you get there, need to think about
    // the whole MPE thing where we should really track a range of CCs related
    // to this note and store them here
    int velocity = 0;
    
    // the original recorded duration of this note
    int originalDuration = 0;

    // the adjusted duration if the note was clipped by a segment or layer boundary
    int duration = 0;

    // remaining number of frames to hold this note
    int remaining = 0;

    // the layer this note came from when playing
    class MidiLayer* layer = nullptr;

    // the event this note came from when recording
    class MidiEvent* event = nullptr;

};

class MidiNotePool : public ObjectPool
{
  public:

    MidiNotePool();
    virtual ~MidiNotePool();

    class MidiNote* newNote();

  protected:
    
    virtual PooledObject* alloc() override;
    
};
