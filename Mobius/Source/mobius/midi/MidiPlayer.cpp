/**
 * Manages the MIDI playback process for a MidiTrack
 *
 * Closely associated with, but not dependent on MidiRecorder.
 * MidiTrack manages the coordination between the two.
 *
 * The Player is much simpler than the Recorder.  Playback position
 * can jump around freely, any internal "cursor" state is expected to
 * adapt to changes in position.
 *
 * The Player is always playing a MidiLayer, and this layer can be changed
 * at any time.  Player does not own the Layer.
 *
 */

#include <JuceHeader.h>

#include "../../util/StructureDumper.h"
#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

// for midiSend
#include "../MobiusInterface.h"

#include "MidiPools.h"
#include "MidiLayer.h"
#include "MidiTrack.h"
#include "MidiFragment.h"

#include "MidiPlayer.h"

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

MidiPlayer::MidiPlayer(MidiTrack* t)
{
    track = t;
}

MidiPlayer::~MidiPlayer()
{
    pools->reclaim(restoredHeld);
    restoredHeld = nullptr;
}

/**
 * Called once during the application initialization process
 * when resources are available.
 */
void MidiPlayer::initialize(MidiPools* p)
{
    pools = p;
    harvester.initialize(p);
}

void MidiPlayer::dump(StructureDumper& d)
{
    d.start("MidiPlayer:");
    d.add("frames", loopFrames);
    d.add("frame", playFrame);
    d.addb("muted", muted);
    d.addb("paused", paused);
    d.newline();

    d.inc();
    if (playLayer != nullptr)
      playLayer->dump(d);
    d.dec();
}

void MidiPlayer::setDeviceId(int id)
{
    outputDevice = id;
}

int MidiPlayer::getDeviceId()
{
    return outputDevice;
}

void MidiPlayer::setChannelOverride(int chan)
{
    channelOverride = chan;
}

//////////////////////////////////////////////////////////////////////
//
// Layer Management
//
//////////////////////////////////////////////////////////////////////

/**
 * Reset all play state.
 * The position returns to zero, and any held notes are turned off.
 *
 * !! what about mute mode?
 */
void MidiPlayer::reset()
{
    // make sure everything we sent in the past is off
    forceOff();
    
    playLayer = nullptr;
    playFrame = 0;
    loopFrames = 0;

    harvester.reset();
    pools->reclaim(restoredHeld);
    restoredHeld = nullptr;

    // didn't do muted at first thinking we would be the holder of the
    // minor mode, but that's too fragile, if Track wants a minor mode for mute
    // it needs to remute after reset
    muted = false;
    paused = false;
}

/**
 * Unlike setLayer, we expect this to have contunity with the last
 * layer so don't need to force notes off.
 *
 * Playback position is set back to zero, Track needs to move it if necessary.
 */
void MidiPlayer::shift(MidiLayer* layer)
{
    // this is not supposed to happen in pause mode
    // either the entire track is paused and won't be recording
    // or we're in Insert mode and won't be shifting
    if (paused)
      Trace(1, "MidiPlayer: Shift requested in Pause mode");
    
    if (layer == nullptr) {
        Trace(1, "MidiPlayer: Can't shift a null layer");
    }
    else {
        playLayer = layer;
        loopFrames = layer->getFrames();

        playFrame = 0;
        playLayer->resetPlayState();
    }
}

/**
 * Here after playing to the end and the track decided not to
 * shift a new layer.  Just start over from the beginning.
 */
void MidiPlayer::restart()
{
    if (paused) return;
    
    playFrame = 0;
    if (playLayer != nullptr)
      playLayer->resetPlayState();
}

/**
 * Install a layer to play.
 * Unlike shift() this is not expected to be a seamless transition.
 * Typically done when using undo, redo, or loop switch.
 * 
 * If a layer was already playing it will turn off held notes.
 *
 * todo: need more thought around "seamless" layer transitions where the next
 * layer can handle NoteOffs for things turned on in this layer.
 * That part may be tricky, this might need to be part of the Layer state
 * "the notes that were held when I was entered".  If a sequence ends with held notes
 * and enters a sequence that turns on those notes, but was created with held notes,
 * the notes can just continue being held and do not need to be retriggered.
 *
 * hmm, don't overthink this, let that be handled in shift() ?
 */
void MidiPlayer::change(MidiLayer* layer, int newFrame)
{
    // checkpoint held notes in case we return here
    saveHeld();
    
    // until we get transitions worked out, changing a layer always closes notes
    forceOff();

    playLayer = layer;
    
    if (layer == nullptr) {
        loopFrames = 0;
    }
    else {
        loopFrames = layer->getFrames();
    }

    if (newFrame == -1) {
        // attempt to keep the same relative location
        newFrame = playFrame;
    }
    
    setFrame(newFrame);

    // we now own the play cursor in this layer
    if (playLayer != nullptr)
      playLayer->resetPlayState();
}

/**
 * When chcanging from one layer to another while playing, capture the current held notes
 * and save them as a layer "checkpoint" so that if we return to that layer we can
 * more easily determine what held notes need to be turned back on.
 *
 * todo: should also be tracking and remembering the last value of any CCs so they
 * too can be restored?  Hmm, maybe not, if they're using CCs as a performance control
 * may want those to just carry over as they bounce between loops and layers.
 */
void MidiPlayer::saveHeld()
{
    if (playLayer != nullptr && heldNotes != nullptr) {

        MidiFragment* existing = playLayer->getNearestCheckpoint(playFrame);
        if (existing != nullptr && existing->frame == playFrame) {
            // already have a checkpoint here, play layers can't change without
            // going back through Recorder which will reset checkpoints
            // so just leave the one there
        }
        else {
            MidiFragment* frag = pools->newFragment();
            for (MidiEvent* e = heldNotes ; e != nullptr ; e = e->next) {
                MidiEvent* copy = pools->newEvent();
                copy->copy(e);
                // the peer is not copied for some reasson, I think it is safe here
                copy->peer = e->peer;
                frag->sequence.add(copy);
            }

            frag->frame = playFrame;

            // todo: might want a governor on how many of these we can accumulate
            // should normally be small unless they are rapidily bouncing between layers
            // or loops as a performance technique
            playLayer->add(frag);
        }
    }
}

/**
 * After changing the playback location, usually after also calling change()
 * determine which notes would have been held if this layer had been playing
 * normally.
 */
void MidiPlayer::prepareHeld()
{
    MidiFragment* held = nullptr;
    
    if (playLayer != nullptr) {
        // todo: should also be including previous segments in this analysis
        // since the segment preset is in effect a held checkpoint
        MidiFragment* checkpoint = playLayer->getNearestCheckpoint(playFrame);
        if (checkpoint != nullptr && checkpoint->frame == playFrame) {
            // we're lucky, returning to the same location we left
            held = pools->copy(checkpoint);
        }
        else {
            // wait, don't need to be looking for checkpoints out here, harvester
            // will use them
            held = harvester.harvestCheckpoint(playLayer, playFrame);
        }

    }
    
    pools->reclaim(restoredHeld);
    restoredHeld = held;
}

//////////////////////////////////////////////////////////////////////
//
// Play State
//
//////////////////////////////////////////////////////////////////////

int MidiPlayer::getFrame()
{
    return playFrame;
}

int MidiPlayer::getFrames()
{
    return loopFrames;
}

bool MidiPlayer::isMuted()
{
    return muted;
}

bool MidiPlayer::isPaused()
{
    return paused;
}

/**
 * Enter a state of Mute.
 * 
 * Held notes are turned off and the mute flag is set.  This flag
 * is the basis for advertising the Mute minor mode in the UI, and
 * for drawing the loop state in blue.
 *
 * This differs from Pause mode because the note durations are allowed
 * to advance.
 */
void MidiPlayer::setMute(bool b)
{
    if (b) {
        if (!muted) {
            // turning mute on
            // todo: either turn everything off then back on
            // or set volume CC to 0 then back to the previous value
            // currently turning off then on
            // note that we don't use forceOff here since that also removes
            // them from the tracking list
            MidiEvent* held = heldNotes;
            while (held != nullptr) {
                sendOff(held);
                held = held->next;
            }
            muted = true;
        }
    }
    else if (muted) {
        // turning mute off
        // turn on any notes that are still being (silently) held
        muted = false;
        MidiEvent* held = heldNotes;
        while (held != nullptr) {
            sendOn(held);
            held = held->next;
        }
    }
}
 
/**
 * Put the player in a state of pause.
 * This can happen for two reasons:
 *    - the Pause function
 *    - the track entering Insert mode
 *
 * In the first case, the track should cease advancing and play() and
 * shift() will not be called.
 *
 * In the second case shift() should not be called but play() might be
 * and should be ignored.
 *
 * In both cases the player is effectively muted, but we do not set the
 * mute flag.  
 *
 * When turned off, held notes are restored, unless the playback location changed.
 * 
 * The noHold option is used with Insert or other operations where we don't
 * want notes held when the pause was started to continue after
 * the unpause.
 */
void MidiPlayer::setPause(bool b, bool noHold)
{
    if (b) {
        if (!paused) {
            // turning pause on
            MidiEvent* held = heldNotes;
            while (held != nullptr) {
                sendOff(held);
                held = held->next;
            }
            paused = true;
        }
    }
    else if (paused) {
        paused = false;
        if (noHold)
          flushHeld();
        else {
            MidiEvent* held = heldNotes;
            while (held != nullptr) {
                sendOn(held);
                held = held->next;
            }
        }
    }
}

/**
 * Stop is similar to pause except it rewinds to zero and
 * flushes held notes.
 */
void MidiPlayer::stop()
{
    setPause(true);
    setFrame(0);
    flushHeld();
}

/**
 * Set the playback position
 * This is usually combined with change() for undo/redo/switch.
 * This can also be used to jump around in the play layer without changing it.
 *
 * todo: this hasn't been shutting notes off, it will need to if this becomes
 * a more general play mover.  If you're jumping a large amount the current
 * held notes normally would need to go off.  If you're jumping just a little within
 * the range of the held notes, then their durations should be adjusted.  If you
 * flush/prepare instead it works but you get an extra retrigger of the notes.
 */
void MidiPlayer::setFrame(int frame)
{
    // todo: should we make a checkpoint here?
    // setFrame is called in more situations than change() so I think no, but
    // in those cases heldNotes should be empty
    
    if (loopFrames == 0) {
        // doesn't matter what they asked for
        playFrame = 0;
    }
    else {
        // wrap within the available frames
        int adjustedFrame = frame;
        if (frame > loopFrames) {
            adjustedFrame = frame % loopFrames;
            Trace(2, "MidiPlayer: Wrapping play frame from %d to %d",
                  frame, adjustedFrame);
        }
        
        playFrame = adjustedFrame;
    }

    if (playLayer != nullptr)
        playLayer->resetPlayState();
    
    // determine which notes would be held at this position
    prepareHeld();
}

//////////////////////////////////////////////////////////////////////
//
// Play/Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * Reset the pseudo level meter at the beginning of each block.
 */
int MidiPlayer::captureEventsSent()
{
    int result = eventsSent;
    eventsSent = 0;
    return result;
}

/**
 * Play anything from the current position forward until the end
 * of the play region.
 */
void MidiPlayer::play(int blockFrames)
{
    // ignored in pause mode
    if (paused) return;
    
    if (blockFrames > 0 && playLayer != nullptr) {

        if (loopFrames == 0) {
            // empty layer
        }
        else if (blockFrames > loopFrames) {
            // sequence was not empty but was extremely short
            // technically we should cycle over the layer more than once, but this
            // complicates things and is most likely an error
            Trace(1, "MidiPlayer: Extremely short loop or extremely large block, take your pick");
            loopFrames = 0;
        }
        else {
            harvester.harvestPlay(playLayer, playFrame, playFrame + blockFrames - 1);

            // these just spray out without fuss
            MidiSequence* events = harvester.getEvents();
            if (events != nullptr) {
                for (MidiEvent* e = events->getFirst() ; e != nullptr ; e = e->next) {
                    track->midiSend(e->juceMessage, outputDevice);
                    eventsSent++;
                }
            }

            // add the restoredHeld notes if we were jumping
            if (restoredHeld != nullptr) {
                MidiEvent* notes = restoredHeld->sequence.steal();
                while (notes != nullptr) {
                    MidiEvent* next = notes->next;
                    play(notes);
                    notes = next;
                }
                pools->reclaim(restoredHeld);
                restoredHeld = nullptr;
            }

            // ownership of held notes will be transferred for duration tracking
            MidiSequence* noteseq = harvester.getNotes();
            if (noteseq != nullptr) {
                MidiEvent* notes = noteseq->steal();
                while (notes != nullptr) {
                    MidiEvent* next = notes->next;
                    play(notes);
                    notes = next;
                }
            }

            // keep this clean between calls
            harvester.reset();
            
            advanceHeld(blockFrames);
            
            playFrame += blockFrames;
        }
    }
}

/**
 * Begin tracking a note and send it to the device if we're not muted.
 * Continue note duration tracking even if we are in mute mode so that
 * if mute is turned off before we've reached the duration it can be
 * turned back on for the remainder.
 * This will obviously have the attack envelope problem.
 *
 * The notes will have been gathered by MidiHarvester and we take
 * ownership of them.
 */
void MidiPlayer::play(MidiEvent* note)
{
    if (note != nullptr) {

        // check this for awhile, I don't think these should happen
        if (note->duration == 0)
          Trace(1, "MidiPlayer: Playing a note with no duration");

        note->remaining = note->duration;
        note->next = heldNotes;
        heldNotes = note;

        sendOn(note);
    }
}

/**
 * Inner sender used by both send() and setMute()
 * This just sends the NoteOn event and doesn't mess with durations which
 * in the case of setMute are already being tracked.
 */
void MidiPlayer::sendOn(MidiEvent* note)
{
    // bump the sent count even if we're muted so we can see the levels
    // flicker
    eventsSent++;
    if (!muted && !paused) {

        if (channelOverride == 0) {
            track->midiSend(note->juceMessage, outputDevice);
        }
        else {
            juce::MidiMessage msg =
                juce::MidiMessage::noteOn(channelOverride,
                                          note->juceMessage.getNoteNumber(),
                                          (juce::uint8)(note->releaseVelocity));
            track->midiSend(msg, outputDevice);
            // remember this in the held note tracking state so we turn
            // off the right one
            note->channelOverride = channelOverride;
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Checkpoints
//
//////////////////////////////////////////////////////////////////////

void MidiPlayer::checkpoint()
{
    MidiFragment* frag = playLayer->getNearestCheckpoint(playFrame);
    if (frag != nullptr && frag->frame == playFrame) {
        // already have one at this location, won't have changed
    }
    else {
        frag = harvester.harvestCheckpoint(playLayer, playFrame);
        playLayer->add(frag);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Note Duration Tracking
//
//////////////////////////////////////////////////////////////////////

/**
 * Release the state of any held note tracking without sending NoteOffs
 */
void MidiPlayer::flushHeld()
{
    while (heldNotes != nullptr) {
        MidiEvent* next = heldNotes->next;
        heldNotes->next = nullptr;
        pools->checkin(heldNotes);
        heldNotes = next;
    }
}

/**
 * Decrease the hold duration for any "on" notes, and when the duration is
 * reached, send a NoteOff.
 *
 * Think about when we advance for notes we just added to the list in the
 * current block.  Do those advance now or on the next block?
 */
void MidiPlayer::advanceHeld(int blockFrames)
{
    MidiEvent* prev = nullptr;
    MidiEvent* held = heldNotes;
    while (held != nullptr) {
        MidiEvent* next = held->next;

        held->remaining -= blockFrames;
        if (held->remaining <= 0) {
            sendOff(held);
            if (prev == nullptr)
              heldNotes = next;
            else
              prev->next = next;
            held->next = nullptr;
            pools->checkin(held);
        }
        else {
            prev = held;
        }

        held = next;
    }
}

/**
 * Force all currently held notes off.
 */
void MidiPlayer::forceOff()
{
    while (heldNotes != nullptr) {
        sendOff(heldNotes);
        MidiEvent* next = heldNotes->next;
        heldNotes->next = nullptr;
        pools->checkin(heldNotes);
        heldNotes = next;
    }
}

/**
 * Send a NoteOff to the device
 */
void MidiPlayer::sendOff(MidiEvent* note)
{
    // is it safe to test mute mode or should we just send a redundant off
    // every time?  when entering mute mode it is supposed to have
    // forced everything off, we continue tracking so we can restore notes
    // when mute is turned off which will call down to sendOff when the (silent)
    // note finishes durating
    if (!muted && !paused) {

        int channel = note->juceMessage.getChannel();
        if (note->channelOverride > 0)
          channel = note->channelOverride;
        
        juce::MidiMessage msg =
            juce::MidiMessage::noteOff(channel,
                                       note->juceMessage.getNoteNumber(),
                                       (juce::uint8)(note->releaseVelocity));
        track->midiSend(msg, outputDevice);

        // shouldn't matter but be clean
        note->channelOverride = 0;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
