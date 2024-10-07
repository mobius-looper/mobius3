/**
 * New MIDI event structure designed for use with MidiTracks
 *
 * Messages come in and go out using juce::MidiMessage, this wraps one of those
 * and provides extra state for MidiTracker.
 */

#pragma once

#include <JuceHeader.h>

#include "../model/ObjectPool.h"

class MidiEvent : public PooledObject
{
  public:

    void poolInit() override;
    void copy(MidiEvent* src);

    /**
     * Chain pointer for sequences.
     * Not the same as the pool chain.
     */
    MidiEvent* next = nullptr;

    /**
     * The device id this event came from.
     * Ids start from 1 and are assigned at runtime.
     */
    int device = 0;

    /**
     * The wrapped Juce message
     */
    juce::MidiMessage juceMessage;

    /**
     * The position in the audio stream where this
     * event was recorded and will be played.
     */
    int frame = 0;

    /**
     * For notes, the duration in frames.
     */
    int duration = 0;

    /**
     * For notes, the release velocity if tracking duration
     */
    int releaseVelocity = 0;

    //
    // Held note state
    //

    int remaining = 0;

};

class MidiEventPool : public ObjectPool
{
  public:

    MidiEventPool();
    virtual ~MidiEventPool();

    class MidiEvent* newEvent();

  protected:
    
    virtual PooledObject* alloc() override;
    
};
