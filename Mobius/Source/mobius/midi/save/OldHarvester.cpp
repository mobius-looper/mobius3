//////////////////////////////////////////////////////////////////////
//
// Play Traversal - first implementation
//
//////////////////////////////////////////////////////////////////////

/**
 * Obtain the events in a Layer within the given range.
 * The range frame numbers relative to the layer itself, with zero being
 * the start of the layer.  The events gathered will also have layer relative
 * frames.  These events are always copies of the underlying recorded events
 * and may be adjusted.
 *
 * startFrame and endFrame are inclusive, meaning if the loop length is 256
 * then the final endFrame will be 255.
 *
 * This may be called recursively and will append to the accumulation lists.
 *
 * A Layer has two things, a MidiSequenece containing events recorded (or flattened into)
 * the layer, and a list of MidiSegments containing references to other layers.
 *
 * I want to keep the traversal logic out of MidiLayer and MidiSegment since they are
 * so closely related and need to be seen together.  To prevent having to search from the
 * beginning each time, this maintains a "seek cache" in the layer since playback
 * will harvest successive regions.  Would be nicer to have a doubly linked
 * list for MidiEvent and MidiSegment.
 */
void MidiHarvester::harvest(MidiLayer* layer, int startFrame, int endFrame)
{
    if (layer->seekFrame != startFrame) {
        // cursor moved or is being reset, reorient
        seek(layer, startFrame);
    }

    // at this point layer->seekFrame will be equal to startFrame
    // layer->seekNextEvent will be the first event from the local sequence that is
    // at or beyond startFrame or null if we've reached the end of the sequence
    // layer->seekNextSegment will be the first (and only since they can't overlap) segment
    // whose range includes or is after startFrame

    MidiEvent* nextEvent = layer->seekNextEvent;
    
    while (nextEvent != nullptr) {

        if (nextEvent->frame <= endFrame) {

            // the layer sequence has 0 relative frames so they go right in
            (void)add(nextEvent);
            
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
            
            harvest(nextSegment, segStartOffset, segEndOffset);

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
 * of the segment.
 *
 * This calls harvest(layer) recursively to do the traversal, then post processes
 * the events that were added to make the adjustments.
 *
 * If the segment has a prefix, all of those are added since they logically happen
 * at the beginning of the segment.
 */
void MidiHarvester::harvest(MidiSegment* segment, int startFrame, int endFrame)
{
    // math sanity checks
    if (startFrame < 0)
      Trace(1, "MidiHarvester: Segment start frame went negative, like your popularity");
    if (endFrame > segment->segmentFrames)
      Trace(1, "MidiHarvester: Segment end frame is beyond where it should be");
    
    if (startFrame == 0) {
        // we've entered the segment, here comes the prefix
        if (segment->prefix != nullptr) {
            MidiEvent* event = segment->prefix->getFirst();
            while (event != nullptr) {
                // the frame on these is usually zero but may be offset within the segment
                MidiEvent* copy = add(event);
                copy->frame += segment->originFrame;
                // we shouldn't have to worry about duration here, since the segment
                // owned it it should already be clipped
                event = event->next;
            }
        }
    }

    // on to the segment's layer
    // here we recurse and harvest the layer with start/end frames adjusted
    // for the segment's reference offset
    
    int layerStart = segment->referenceFrame + startFrame;
    int layerEnd = segment->referenceFrame + endFrame;
    // remember the start of the added notes
    int firstNoteIndex = notes.size();
    int firstOtherIndex = events.size();
    harvest(segment->layer, layerStart, layerEnd);

    // the events that were just added were relative to the referenced layer
    // these now need to be pushed upward to be relative to the segment
    // within the containing layer
    // also too, clip any durations that extend past the segment
    // again, I'm preferring inclusive frame numbers rather than "one after the end"
    // just to be consistent 
    int seglast = segment->originFrame + segment->segmentFrames - 1;
    for (int i = firstNoteIndex ; i < notes.size() ; i++) {
        MidiEvent* note = notes[i];
        note->frame += segment->originFrame;
        // the frame containing the last lingering of this note
        int noteLast = note->frame + note->duration - 1;
        if (noteLast > seglast) {
            // it went past the segment boundary, back it up
            note->duration = seglast - note->frame + 1;
            // sanity check because you're bad at math or left zero length things behind
            if (note->duration <= 0) {
                Trace(1, "MidiHarvester: Correcting collapsed duration because you suck at math");
                note->duration = 1;
            }
        }
    }

    // same for cc events except we don't have to mess with durations
    for (int i = firstOtherIndex ; i < events.size() ; i++) {
        MidiEvent* event = events[i];
        event->frame += segment->originFrame;
    }
}

/**
 * Add an event to one of the arrays.
 * Frame and duration adjustments happen later in harvest(segment)
 */
MidiEvent* MidiHarvester::add(MidiEvent* e)
{
    MidiEvent* copy = nullptr;
    
    if (e != nullptr) {
        if (e->juceMessage.isNoteOff()) {
            // shouldn't be recording these any more, Recorder must be tracking
            // durations isntead
            Trace(1, "MidiHarvester: Encountered NoteOff event, what's the deal?");
        }
        else if (e->juceMessage.isNoteOn()) {
            // this is where I'd like to filter notes that don't extend beyond
            // the segment start frame, but when descending into nested segments
            // we don't have enough information at this point to know what location
            // this event will be in at the end
            copy = midiPool->newEvent();
            copy->copy(e);
            notes.add(copy);
        }
        else if (!heldNotesOnly) {
            copy = midiPool->newEvent();
            copy->copy(e);
            events.add(copy);
        }
    }
    return copy;
}

