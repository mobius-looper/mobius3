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

    MidiHarvester();
    ~MidiHarvester();
    void initialize(class MidiPools* pools);

    void reset();

    class MidiSequence* getNotes() {
        return &playNotes;
    }

    class MidiSequence* getEvents() {
        return &playEvents;
    }

    /**
     * Obtain the events in a Layer within the given range.
     * The range frame numbers relative to the layer itself, with zero being
     * the start of the layer.  The events gathered will also have layer relative
     * frames.  These events are always copies of the underlying recorded events
     * and may be adjusted.  The results are available with the getNotes
     * and getEvents methods.
     */
    void harvestPlay(class MidiLayer* layer, int startFrame, int endFrame);

    /**
     * A specialized form of harvesting that calculates the held note prefix
     * for a segment.
     */
    void harvestPrefix(class MidiSegment* segment);

    /**
     * Similar to a prefix, need to share more
     */
    class MidiFragment* harvestCheckpoint(class MidiLayer* layer, int frame);
    
  private:

    class MidiPools* pools = nullptr;

    // always need these so don't bother with the pool
    class MidiSequence playNotes;
    class MidiSequence playEvents;

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
    
    void decay(class MidiSequence* seq, int blockSize);

};
    
