/**
 * What the harvester does isn't conceptually that complicated but the math
 * involved is subtle and very easy to get wrong.  The code has more steps and
 * calculations than necessary but I'm going for clarity rather than brevity.
 *
 */
 
#include "../../util/Trace.h"
#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

#include "MidiLayer.h"
#include "MidiSegment.h"

#include "MidiHarvester.h"

MidiHarvester::MidiHarvester()
{
}

MidiHarvester::~MidiHarvester()
{
    // probably not errors, but things should be keeping this clean
    if (playNotes.size() > 0)
      Trace(1, "MidiHarverster: Lingering notes at destruction");
        
    if (playEvents.size() > 0)
      Trace(1, "MidiHarverster: Lingering events at destruction");
      
    if (midiPool != nullptr) {
        playNotes.clear(midiPool);
        playEvents.clear(midiPool);
    }
}

void MidiHarvester::initialize(MidiEventPool* epool, MidiSequencePool* spool)
{
    midiPool = epool;
    sequencePool = spool;
}

void MidiHarvester::reset()
{
    playNotes.clear(midiPool);
    playEvents.clear(midiPool);
}

//////////////////////////////////////////////////////////////////////
//
// Range Harvest
//
//////////////////////////////////////////////////////////////////////

/**
 * Harvest a block of events in a layer leaving results in a sequence.
 * This does not do hold detection from prior ranges.
 *
 * This is the core harvesting implementation used by both harvestPlay
 * and harvestPrefix.
 *
 * The heldOnly flag will cause the filtering of notes that do not carry over the
 * end of the region.  This is set only for prefix calculation.
 *
 * The forceFirstPrefix is used to force inclusion of the first segment prefix
 * even if the previous one was adjacent.  This is set for prefix calculation
 * and for playback when the play cursor moves to a random location.
 *
 */
void MidiHarvester::harvestRange(MidiLayer* layer, int startFrame, int endFrame,
                                 bool heldOnly, bool forceFirstPrefix,
                                 MidiSequence* noteResult, MidiSequence* eventResult)
{
    // first the layer sequence
    if (layer->seekFrame != startFrame) {
        // cursor moved or is being reset, reorient
        seek(layer, startFrame);
    }

    MidiEvent* nextEvent = layer->seekNextEvent;
    while (nextEvent != nullptr) {

        if (nextEvent->frame <= endFrame) {

            int eventLast = nextEvent->frame + nextEvent->duration - 1;

            if (heldOnly) {
                // only if it extends beyond this block
                // this is kind of unnecessary, the block size for prefix
                // evaluation will tend to be short and the note will extend anway
                // if you avoid this check, it just means very short held notes will almost
                // immediately decay anyway
                if (eventLast > endFrame)
                  (void)add(nextEvent, heldOnly, noteResult, eventResult);
            }
            else {
                // always add it
                (void)add(nextEvent, heldOnly, noteResult, eventResult);
            }
            
            nextEvent = nextEvent->next;
        }
        else {
            // next not in range, stop
            break;
        }
    }

    // now the segments
    MidiSegment* nextSegment = layer->seekNextSegment;

    while (nextSegment != nullptr) {

        int segstart = nextSegment->originFrame;
        int seglast = segstart + nextSegment->segmentFrames - 1;

        if (segstart > startFrame) {
            // haven't reached this segment yet, wait for the next block
            break;
        }
        else if (seglast < startFrame) {
            // this segment has passed, seek must be broken
            Trace(1, "MidiHarvester: Unexpected past segment in cursor");
            nextSegment = nullptr;
        }
        else {
            // segment in range
            // scale the start/end into the segment
            int segStartOffset = startFrame - nextSegment->originFrame;
            if (segStartOffset < 0) {
                // the harvest frame is a little before the segment, since
                // segments can't overlap, this is dead space we can skip over
                segStartOffset = 0;
                startFrame = nextSegment->originFrame;
            }
                
            int segEndOffset = endFrame - nextSegment->originFrame;

            if (segEndOffset > seglast) {
                // this segment is too short for the requested region
                segEndOffset = seglast;
            }
            
            harvest(nextSegment, segStartOffset, segEndOffset, heldOnly, forceFirstPrefix,
                    noteResult, eventResult);

            // this now goes off
            forceFirstPrefix = false;

            // advance the harvest start frame for what we took from this
            // segment
            startFrame += (segEndOffset - segStartOffset + 1);

            if (seglast <= endFrame) {
                // segment has been consumed, move to the next one
                nextSegment = nextSegment->next;
            }
            else {
                // more to go in this segment
                break;
            }
        }
    }

    // remember the seek advance for next time
    layer->seekFrame = endFrame + 1;
    layer->seekNextEvent = nextEvent;
    layer->seekNextSegment = nextSegment;
}

/**
 * Orient the play cursor to include the given range.
 */
void MidiHarvester::seek(MidiLayer* layer, int startFrame)
{
    MidiEvent* nextEvent = nullptr;
    
    MidiSequence* sequence = layer->getSequence();
    if (sequence != nullptr) {
        MidiEvent* event = sequence->getFirst();
        while (event != nullptr) {
            if (event->frame < startFrame)
              event = event->next;
            else
              break;
        }
        nextEvent = event;
    }

    MidiSegment* seg = layer->getSegments();
    while (seg != nullptr) {

        int segstart = seg->originFrame;
        int seglast = segstart + seg->segmentFrames - 1;

        if (seglast < startFrame) {
            // segment is in the past
            seg = seg->next;
        }
        else {
            // segment is either in the future or spans the play frame
            break;
        }
    }

    // save seek state
    layer->seekFrame = startFrame;
    layer->seekNextEvent = nextEvent;
    layer->seekNextSegment = seg;
}

/**
 * Harvest events covered by a segment.
 * Now it gets more complex.
 * 
 * startFrame and endFrame are relative to the segment.
 * This range must be converted to the corresponding range in the underlying layer
 * relative to the Segment's referenceFrame.
 *
 * Events returned have their frame adjusted to be relative to the layer containing
 * the segment.
 *
 * Events returned have their duration adjusted so they do not exceed the bounds
 * of the segment UNLESS the segment after this one is logically adjacent in time.
 *
 * This calls harvest(layer) recursively to do the traversal, then post processes
 * the events that were added to make the adjustments.
 *
 * If the segment has a prefix, all of those are added since they logically happen
 * at the beginning of the segment.
 * !! no, not if there is continuity with the previous segment
 */
void MidiHarvester::harvest(MidiSegment* segment, int startFrame, int endFrame,
                            bool heldOnly, bool forcePrefix,
                            MidiSequence* noteResult, MidiSequence* eventResult)
{
    // math sanity checks
    if (startFrame < 0)
      Trace(1, "MidiHarvester: Segment start frame went negative, like your popularity");
    if (endFrame > segment->segmentFrames)
      Trace(1, "MidiHarvester: Segment end frame is beyond where it should be");
    
    int seglast = segment->originFrame + segment->segmentFrames - 1;
    
    if (startFrame == 0) {
        // we've entered the segment, add the prefix unless there was continunity
        // with the previous segment, or this the first one after some kind of jump
        if (segment->prefix.size() > 0 && (forcePrefix || !hasContinuity(segment))) {
            MidiEvent* event = segment->prefix.getFirst();
            while (event != nullptr) {
                // the frame on these is usually zero but may be offset within the segment

                MidiEvent* copy = add(event, heldOnly, noteResult, eventResult);
                if (copy != nullptr) {
                    // why would a prefix note have a non-zero frame?
                    copy->frame += segment->originFrame;

                    // kludge: prefix notes aren't post-processed like the nestedNotes
                    // below to clip them on a segment boundary
                    // we could move nestedNotes up and make them all behave the same (sounds right)
                    // or we could say that prefix notes should have been clipped even
                    // before we got here (good idea, but hard to control), had a bug
                    // with cascading replace where shortening a segment didn't also adjust
                    // the prefix duration
                    //
                    // but if we clip here, then we don't really need to decay during
                    // prefix calculation at all, they're really just continuations of the
                    // original note?  hmm, not really because the did have their start
                    // frame logically moved, whatever the decision there it is important
                    // that we clip prefix notes like nested notes
                    //
                    // update: no, I like leaving the note in it's original calculated duration
                    // and clipping it here, it makes sense because it's like the note
                    // was played at the beginning of this segment and it has whatever duration
                    // it had, but is clipped
                    // rework this so that clipping is handled consistently at the end

                    if (!hasContinuity(segment)) {
                        // the frame containing the last lingering of this note
                        int noteLast = copy->frame + copy->duration - 1;
                        if (noteLast > seglast) {
                            // it went past the segment boundary, back it up
                            copy->duration = seglast - copy->frame + 1;
                            // sanity check because you're bad at math or left zero length things behind
                            if (copy->duration <= 0) {
                                Trace(1, "MidiHarvester: Correcting collapsed duration because you suck at math");
                                copy->duration = 1;
                            }
                        }
                    }
                }
                    
                event = event->next;
            }
        }
    }

    // on to the segment's layer
    // here we recurse and harvest the layer with start/end frames adjusted
    // for the segment's reference offset
    
    int layerStart = segment->referenceFrame + startFrame;
    int layerEnd = segment->referenceFrame + endFrame;
    // accumulate these in a temporary sequence we can post-process
    MidiSequence nestedNotes;
    MidiSequence nestedEvents;

    // passing a null event result means to ignore those
    MidiSequence* nestedEventResult = (eventResult != nullptr) ? &nestedEvents : nullptr;
    
    // !! what about forcePrefix here?
    harvestRange(segment->layer, layerStart, layerEnd, heldOnly, false, &nestedNotes, nestedEventResult);

    // the events that were just added were relative to the referenced layer
    // these now need to be pushed upward to be relative to the segment
    // within the containing layer
    // also too, clip any durations that extend past the segment if there is NOT
    // continuity with the next segment
    // again, I'm preferring inclusive frame numbers rather than "one after the end"
    // just to be consistent 
    MidiEvent* nested = nestedNotes.getFirst();
    while (nested != nullptr) {
        nested->frame += segment->originFrame;
        if (!hasContinuity(segment)) {
            // the frame containing the last lingering of this note
            int noteLast = nested->frame + nested->duration - 1;
            if (noteLast > seglast) {
                // it went past the segment boundary, back it up
                nested->duration = seglast - nested->frame + 1;
                // sanity check because you're bad at math or left zero length things behind
                if (nested->duration <= 0) {
                    Trace(1, "MidiHarvester: Correcting collapsed duration because you suck at math");
                    nested->duration = 1;
                }
            }
        }
        nested = nested->next;
    }
    noteResult->append(&nestedNotes);

    // same for cc events except we don't have to mess with durations
    if (eventResult != nullptr) {
        nested = nestedEvents.getFirst();
        while (nested != nullptr) {
            nested->frame += segment->originFrame;
            nested = nested->next;
        }
        eventResult->append(&nestedEvents);
    }
}

/**
 * If a segment starts immediately after the previous segment ends,
 * both in layer origin, and in reference frames, it is continuous.
 */
bool MidiHarvester::hasContinuity(MidiSegment* segment)
{
    MidiSegment* prev = segment->prev;
    return (prev != nullptr &&
            (((prev->originFrame + prev->segmentFrames) ==
              segment->originFrame) &&
             ((prev->referenceFrame + prev->segmentFrames) ==
              segment->referenceFrame)));
}

/**
 * Add an event to one of the arrays.
 * Frame and duration adjustments happen later in harvest(segment)
 */
MidiEvent* MidiHarvester::add(MidiEvent* e, bool heldOnly,
                              MidiSequence* noteResult, MidiSequence* eventResult)
{
    MidiEvent* copy = nullptr;
    
    if (e != nullptr) {
        if (e->juceMessage.isNoteOff()) {
            // shouldn't be recording these any more, Recorder must be tracking
            // durations isntead
            Trace(1, "MidiHarvester: Encountered NoteOff event, what's the deal?");
        }
        else if (e->juceMessage.isNoteOn()) {
            copy = midiPool->newEvent();
            copy->copy(e);
            copy->peer = e;

            if (heldOnly) {
                // we're doing prefix calculation, set the decay
                copy->remaining = copy->duration;
            }
            
            noteResult->add(copy);
        }
        else if (eventResult != nullptr) {
            copy = midiPool->newEvent();
            copy->copy(e);
            eventResult->add(copy);
        }
    }
    return copy;
}

//////////////////////////////////////////////////////////////////////
//
// Playback Harvest
//
//////////////////////////////////////////////////////////////////////

/**
 * This interface is used by MidiPlayer to harvest successive ranges
 * of events as each auto block comes in.
 */
void MidiHarvester::harvestPlay(MidiLayer* layer, int startFrame, int endFrame)
{
    reset();
    
    // !! forceFirstPrefix needs to be passed if we're jumping
    // the play frame
    harvestRange(layer, startFrame, endFrame, false, false,
                 &playNotes, &playEvents);
}
    
//////////////////////////////////////////////////////////////////////
//
// Prefix Harvest
//
//////////////////////////////////////////////////////////////////////

/**
 * A specialized form of harvesting used to calculate notes that remain
 * held prior to the start of a segment.
 *
 * Holds are detected by doing a range harvest starting from the beginning of the
 * previous segment forward.  This assumes the previous segment has a properly
 * calculated hold prefix.  
 *
 * If we don't have a previous segment we harvest from the beginning
 * of the backing layer.
 */
void MidiHarvester::harvestPrefix(MidiSegment* segment)
{
    reset();
    
    int startFrame = 0;
    if (segment->prev != nullptr)
      startFrame = segment->prev->originFrame;

    int endFrame = segment->originFrame;

    // block size needs to be large enough to gain some traversal effeciency
    // but not so large that we end up excessively copying notes we decide
    // not to use
    int blockSize = 1024;
    int remaining = endFrame - startFrame + 1;

    MidiSequence heldNotes;
    while (remaining > 0) {

        if (remaining < blockSize)
          blockSize = remaining;

        // decay previous notes
        decay(&heldNotes, blockSize);

        // add new ones
        harvestRange(segment->layer, startFrame, startFrame + blockSize - 1,
                     true, true, &heldNotes, nullptr);

        startFrame += blockSize;
        remaining -= blockSize;
    }
    
    // what remains is the segment prefix
    segment->prefix.clear(midiPool);
    if (heldNotes.size() > 0) {

        // the prefix notes will all start at frame 0 relative
        // to the segment, and the duration will be the remainder of the decay
        // we used MidiEvent::remaining for the decay like Player does but
        // we could have just as well used duration
        MidiEvent* held = heldNotes.getFirst();
        while (held != nullptr) {
            held->frame = 0;
            held->duration = held->remaining;
            held->remaining = 0;
            held = held->next;
        }
        
        segment->prefix.append(&heldNotes);
    }
}

void MidiHarvester::decay(MidiSequence* seq, int blockSize)
{
    MidiEvent* note = seq->getFirst();
    while (note != nullptr) {
        MidiEvent* next = note->next;
        note->remaining -= blockSize;
        if (note->remaining <= 0)
          seq->remove(midiPool, note);
        note = next;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
