/**
 * Utility class that traverses the layer/segment hierarchy gathering
 * events that are to be played within the current audio block.
 *
 * The traversal logic is mostly inside the MidiLayer and MidiSegment classes,
 * Harvester just provides the place to put results.
 *
 * I kind of wanted Harvester to have the traversal logic as well but this requires
 * direct access to internal structures in the layer/segment and especially complicates
 * the maintenance of the layer's "play cursor" which keeps track of where the last
 * gather happened so we can avoid starting from the front every time.
 *
 * One of these will be maintained by each MidiPlayer.  Besides providing the arrays
 * to deposit events, it also provides access to the object pools which segments need
 * when depositing Notes.
 *
 * The split between Note events and all others is necessary so that each segment
 * can do note duration adjustments without modifying the original MidiEvent.
 *
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
    const int MaxEvents = 256;

    MidiHarvester();
    ~MidiHarvester();
    void initialize(class MidiNotePool* npool);

    void reset();

    void add(class MidiEvent* e, int maxExtent);
    
    juce::Array<class MidiEvent*>& getEvents() {
        return events;
    }
    
    juce::Array<class MidiNote*>& getNotes() {
        return notes;
    }

  private:

    class MidiNotePool* notePool = nullptr;
    
    juce::Array<class MidiEvent*> events;
    juce::Array<class MidiNote*> notes;
};
    
