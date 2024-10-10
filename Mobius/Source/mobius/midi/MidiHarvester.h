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
    void initialize(class MidiEventPool* epool, class MidiSequencePool* spool, int capacity);

    void reset();

    juce::Array<class MidiEvent*>& getNotes() {
        return notes;
    }

    juce::Array<class MidiEvent*>& getEvents() {
        return events;
    }

    class MidiSequence* getPlayNotes() {
        return playNotes;
    }

    class MidiSequence* getPlayEvents() {
        return playEvents;
    }

    /**
     * Obtain the events in a Layer within the given range.
     * The range frame numbers relative to the layer itself, with zero being
     * the start of the layer.  The events gathered will also have layer relative
     * frames.  These events are always copies of the underlying recorded events
     * and may be adjusted.
     */
    void harvest(MidiLayer* layer, int startFrame, int endFrame);

    //
    // New Interface, replace the older play interface eventually
    //
    
    void harvestPlay(class MidiLayer* layer, int startFrame, int endFrame);
    void harvestPrefix(class MidiSegment* segment);
    
  private:

    class MidiEventPool* midiPool = nullptr;
    class MidiSequencePool* sequencePool = nullptr;
    int noteCapacity = 0;
    bool heldNotesOnly = false;
    
    juce::Array<class MidiEvent*> notes;
    juce::Array<class MidiEvent*> events;
    class MidiSequence *playNotes = nullptr;
    class MidiSequence *playEvents = nullptr;

    void harvestRange(class MidiLayer* layer, int startFrame, int endFrame,
                      bool heldOnly, bool forceFirstPrefix,
                      class MidiSequence* noteResult, class MidiSequence* eventResult);

    void seek(class MidiLayer* layer, int startFrame);
    
    void harvest(class MidiSegment* segment, int startFrame, int endFrame,
                 bool heldOnly, bool forceFirstPrefix,
                 class MidiSequence* noteResult, class MidiSequence* eventResult);
    
    bool hasContinuity(class MidiSegment* segment);
    
    class MidiEvent* add(class MidiEvent* e, bool heldOnly,
                         class MidiSequence* noteResult, class MidiSequence* eventResult);
    
    void reclaim(class MidiSequence* seq);
    void decay(class MidiSequence* seq, int blockSize);

    // old, temporary

    void harvest(class MidiSegment* segment, int startFrame, int endFrame);
    class MidiEvent* add(class MidiEvent* e);

};
    
