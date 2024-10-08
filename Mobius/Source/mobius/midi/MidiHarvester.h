/**
 * Utility class that traverses the layer/segment hierarchy gathering
 * events.
 *
 * This is used for two things: to gather events for playback on each audio block,
 * and to calculate the "segment prefix" containing notes that are being held into
 * a segment, but do not start in that segment.
 *
 * One of these will be maintained by MidiPlayer for playback and by MidiRecorder
 * for segment prefixes.
 *
 * It could also become a general purpose "flattener" should layer flatting become a thing.
 */

#pragma once

#include <JuceHeader.h>

class MidiHarvester
{
  public:

    /**
     * The maximum number of events we will accumulate in one block for playback
     * This determines the pre-allocated size of the event/note buckets and should be
     * large enough to avoid memory allocation.  Harvester will allow this to be
     * exceeded, but it should be rare and it will whine in the log about it.
     */
    const int DefaultCapacity = 256;

    MidiHarvester();
    ~MidiHarvester();
    void initialize(class MidiEventPool* epool, int capacity);

    void reset();

    /**
     * Obtain the events in a Layer within the given range.
     * The range frame numbers relative to the layer itself, with zero being
     * the start of the layer.  The events gathered will also have layer relative
     * frames.  These events are always copies of the underlying recorded events
     * and may be adjusted.
     */
    void harvest(MidiLayer* layer, int startFrame, int endFrame);
    
    juce::Array<class MidiEvent*>& getNotes() {
        return notes;
    }

    juce::Array<class MidiEvent*>& getEvents() {
        return events;
    }
    
  private:

    class MidiEventPool* eventPool = nullptr;
    int noteCapacity = 0;
    
    juce::Array<class MidiEvent*> notes;
    juce::Array<class MidiEvent*> events;

    void seek(class MidiLayer* layer, int startFrame);
    void harvest(class MidiSegment* segment, int startFrame, int endFrame);
    class MidiEvent* add(class MidiEvent* e);

};
    
