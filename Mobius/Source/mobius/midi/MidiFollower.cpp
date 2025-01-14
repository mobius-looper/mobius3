/**
 * This is not a subcomponent, it is just MidiTrack method implementations
 * related to leader/follower.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/StructureDumper.h"
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../../model/Symbol.h"
#include "../../model/Session.h"
#include "../../model/SymbolId.h"
// for GroupDefinitions
#include "../../model/MobiusConfig.h"

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"
#include "../../script/MslWait.h"

#include "../track/LogicalTrack.h"
#include "../track/BaseScheduler.h"
#include "../track/LooperScheduler.h"
// necessary for a few subcomponents
#include "../track/TrackManager.h"
#include "../track/TrackProperties.h"

#include "MidiPools.h"
#include "MidiLoop.h"
#include "MidiLayer.h"
#include "MidiSegment.h"

#include "MidiTrack.h"

//////////////////////////////////////////////////////////////////////
//
// Resize and Clips
//
//////////////////////////////////////////////////////////////////////

/**
 * Calculate a playback rate that allows two loops to remain in sync
 * with the least amount of (musically useful) change.
 *
 * Because I suck at math, the calculations here are not optimized, but blown
 * out and over-commented so I can remember what the fuck this is doing years
 * down the road when I'm tired and don't want to think too hard.
 *
 * In the simple case the rate is simply dividing one length by another.
 * Each loop repeats exactly once with one slower than the other.  If one
 * loop is significantly larger than the other, this is almost never what you
 * want.  Intead it is desireable to allow the smaller loop to repeat some integral number
 * of times, then apply rate scaling to allow the total number of repetitions
 * to "fill" the larger loop.
 *
 * For example, one loop is 20 seconds long and the other is 4.
 * If the loop we want to stretch is the 20 second loop then the rate would be 20/4 = 5 meaning
 * if the loop plays 5 times as fast the 20 seconds drops to 4.
 *
 * But when dealing with music, you rarely want uneven numbers of repetitions.  5 repeats
 * will stay in sync but the tempo of the recorded rhythm may not match.  Usually
 * it is better to keep the repetitions to a multiple of 2: 1, 2, 4, 8 etc.  Then if the
 * loop is too fast or slow you can use HalfSpeed or DoubleSpeed to adjust it.
 *
 * There are lots of options that could be applied here to tune it for the best results.
 *    - allow odd numbers
 *    - allow 6 or 10 or other factors that are not powers of 2
 *    - allow long loops to be cut in half before scaling
 *
 * Keeping it simple with powers of 2 for now.
 */
float MidiTrack::followLeaderLength(int myFrames, int otherFrames)
{
    // the base rate with no repetitions
    rate = (float)myFrames / (float)otherFrames;

    // allow repetitions to make the rate smaller
    float adjusted = rate;
    if (myFrames > otherFrames) {
        // we are larger and the rate is above 1
        // drop it by half until we are closest to 1 without going below
        float next = adjusted / 2.0f;
        while (next > 1.0f) {
            adjusted = next;
            next = next / 2.0f;
        }
    }
    else {
        // we are smaller and the rate is less than 1
        // double it until we are cloest to 1 without going over
        float next = adjusted * 2.0f;
        while (next < 1.0f) {
            adjusted = next;
            next *= 2.0f;
        }
    }
    rate = adjusted;

    return rate;
}

/**
 * Adapt to a location change in the leader loop.
 *
 * Because I suck at math, the calculations are rather drawn out and
 * commented and use more than the necessary steps, but helps clarify
 * exactly what is going on.
 *
 * The rate is a scaling factor that has already been calculated to allow
 * the two loops to have the same "size" while allowing one or the other
 * to repeat some number of times.
 *
 * If the leader is larger than the follower (us) then we are
 * repeating some number of times (maybe 1) at this rate.  When
 * the leader changes location, it's relatively simple, scale the
 * leader location into our time, and wrap if it exceeds our length
 * (meaning we have been repeating).  It doesn't matter where we are now.
 *
 * If the leader is smaller than us and has been repeating, then our
 * current location is significant since we might want to make the smallest
 * jump in playback position to remain in sync.  There are two options:
 * Favor Early and Favor Late
 *
 * With Favor Early, we simply move our location to the lowest frame that matches
 * where the leader is now.  If we had been playing toward the end of our loop
 * after the leader repeated a few times, this will result in a large jump backward
 * but we remain in sync, we just start our repeats from the beginning.  You might
 * want this if you consider switching loops to be "starting over" in time.
 *
 * With Favor Late, we want to move our location the smallest amount to find
 * where we would have been if the leader had been allowed to repeat.
 *
 * There are in-between cases.  If the leaader repeats 4 times for our length
 * then when the leader jumps we could locate relative to any one of those repetitions,
 * But it feels like the first or last repetition are the most predictable.
 *
 * Finally, if we are starting from an empty loop or have otherwise not been following
 * anything our current playback position is not relevant.  Move to a location that
 * fits toward the end of the leader so we hit the downbeats at the next leader start point.
 */
int MidiTrack::followLeaderLocation(int myFrames, int myLocation,
                                    int otherFrames, int otherLocation,
                                    float playbackRate,
                                    bool ignoreCurrent, bool favorLate)
{
    int newLocation = 0;

    // for the calculations below a default rate of 0.0 means "no change"
    if (playbackRate == 0.0f) playbackRate = 1.0f;

    // when ending a recording, otherLocation will normally be the same
    // will normally be the same as otherFrames, or "one past" the end
    // of the loop which will immediately wrap back to zero after the
    // notification
    if (otherLocation == otherFrames)
      otherLocation = 0;

    if (otherLocation > otherFrames) {
        // this is odd and unexpected
        Trace(1, "MidiTrack: Leader location was beyond the end");
    }
    
    if (myFrames < otherFrames) {
        // we are smaller than the other loop and are allowed to repeat
        // this is where we would be relative to the other loop
        int scaledLocation = (int)((float)(otherLocation) * playbackRate);
        if (scaledLocation > myFrames) {
            // we have been repeating to keep up, wrap it
            newLocation = scaledLocation % myFrames;
        }
        else {
            // we have not been repeating, just go there
            newLocation = scaledLocation;
        }
    }
    else {
        // we are larger than the other loop, and it has been repeating
        if (!favorLate) {
            // just scale the other location
            newLocation = (int)((float)(otherLocation) * playbackRate);
        }
        else if (!ignoreCurrent) {
            // this is where we logically are in the other loop with repeats
            float unscaledLocation = (float)myLocation / playbackRate;
            // this is how many times the other loop has to repeat to get there
            int repetition = (int)(unscaledLocation / (float)(otherFrames));
            // this is how long each repetition of the other loop represents in our time
            float scaledRepetitionLength = (float)(otherFrames) * playbackRate;
            // this is where we would be when the other loop repeats that number of times
            float scaledBaseLocation = scaledRepetitionLength * (float)repetition;
            // this is where we would be in the first repetition
            float scaledOffset = (float)(otherLocation) * playbackRate;
            // this is where we should be
            newLocation = (int)(scaledBaseLocation + scaledOffset);
        }
        else {
            // this is where we would be in the first repetition
            float scaledOffset = (float)(otherLocation) * playbackRate;
            // this is how long each repetition of the other loop represents in our time
            float scaledRepetitionLength = (float)(otherFrames) * playbackRate;
            // increase our location until we're within the last iteration of the leader
            if (scaledRepetitionLength == 0.0f) {
                // shouldn't happen but prevent infinite loop
                Trace(1, "MidiTrack: Repetition rate scaling anomoly");
            }
            else {
                float next = scaledOffset + scaledRepetitionLength;
                while (next < (float)myFrames) {
                    scaledOffset = next;
                    next += scaledRepetitionLength;
                }
                newLocation = (int)scaledOffset;
            }
        }
    }
    
    return newLocation;
}

/**
 * Here after being informed that the leader has changed size and we have not
 * been changed.  Called by our own leaderRecordEnd as well as a few places in
 * Scheduler.
 *
 * This does both a rate shift to scale follower so it plays in sync with the leader,
 * and attempts to carry over the current playback position.
 *
 * todo: Because rate shift applies floating point math, there can be roundoff errors that
 * result in a frame or two of error at the loop point.  When this happens the goal frame
 * could be used to inject or insert "time" to make the MIDI loop stay in sync with the other
 * track it is trying to match.
 *
 * Should we eventually support RateShift/Halfspeed and the other audio track functions
 * there will be conflict with a single playback rate if you use both RateShift
 * and Resize.  Will need to combine those and have another scaling factor,
 * perpahs rateShift and resizesShift that can be multiplied together.
 */
void MidiTrack::leaderResized(TrackProperties& props)
{
    if (props.invalid) {
        // something didn't do it's job and didn't check track number ranges
        Trace(1, "MidiTrack: Resize with invalid track properties");
    }
    else if (props.frames == 0) {
        // the other track was valid but empty
        // we don't resize for this
        Trace(2, "MidiTrack: Resize requested against empty track");
    }
    else {
        int myFrames = recorder.getFrames();
        if (myFrames == 0) {
            Trace(2, "MidiTrack: Resize requested on empty track");
        }
        else if (myFrames == props.frames) {
            // nothing to do
        }
        else {
            rate = followLeaderLength(myFrames, props.frames);
            goalFrames = props.frames;

            // !! need to be considering whether ignoreCurrent should be set here
            // if we had not been following and are suddenly trying to resize, our
            // current location doesn't matter
            int adjustedFrame = followLeaderLocation(myFrames, recorder.getFrame(),
                                                     props.frames, props.currentFrame,
                                                     rate, false, true);
                                                     
            // sanity check, recorder/player should be advancing
            // at the same rate until we start dealing with latency
            // !! not if we're doing Insert
            int playFrame = player.getFrame();
            int recordFrame = recorder.getFrame();
            if (playFrame != recordFrame)
              Trace(1, "MidiTrack: Unexpected record/play frame mismatch %d %d", recordFrame, playFrame);

            // resizing is intended for read-only backing tracks, but it is possible there
            // were modifications made during the current iteration
            // making the recorder go back in time is awkward because I'm not sure if it expects
            // the record position to jump around, append vs. insert on the MidiSequence and leaving
            // modes unfinished
            // could auto-commit and shift now, or just prevent it from moving
            // maybe this should be more like Realign where it waits till the start point
            // of the leader loop and changes then, will want that combined with pause/unpause anyway
            if (recorder.hasChanges()) {
                Trace(1, "MidiTrack: Preventing resize relocation with pending recorder changes");
            }
            else {
                char buf[128];
                snprintf(buf, sizeof(buf), "%f", rate);
                Trace(2, "MidiTrack: Resize rate %s %d local frames %d follow frames",
                      buf, myFrames, props.frames);
                Trace(2, "MidiTrack: Follow frame %d adjusted to local frame %d",
                      props.currentFrame, adjustedFrame);

                recorder.setFrame(adjustedFrame);
                player.setFrame(adjustedFrame);
            }
        }
    }
}

/**
 * Called when we've been informed that the leader has changed location but not
 * it's size.
 */
void MidiTrack::leaderMoved(TrackProperties& props)
{
    (void)props;
    Trace(1, "MidiTrack: leaderMoved not implemented");
}

/**
 * Here after we have changed in some way and may need to adjust our playback rate to
 * stay in sync with the leader.  This is mostly for loop switch and undo/redo, but
 * in theory applies to unrounded multiply/insert or anything else that changes the
 * follower's size.
 *
 * This only adjusts the playback rate, not the location.
 */
void MidiTrack::followLeaderSize()
{
    // ignore if we're empty
    int myFrames = recorder.getFrames();
    if (myFrames > 0) {
    
        // ignore if we don't have an active leader track
        // !! not enough for host/midi leaders
        int leaderTrack = scheduler.findLeaderTrack();
        if (leaderTrack > 0) {
            TrackProperties props;
            manager->getTrackProperties(leaderTrack, props);
            if (props.invalid) {
                Trace(1, "MidiTrack: followLeaderSize() was given an invalid audio track number %d", leaderTrack);
            }
            else if (props.frames == 0) {
                // leader is empty, just continue with what we have now
            }
            else if (myFrames == props.frames) {
                // don't have to adjust rate, but we could factor in cycle counts
                // if that makes sense to make them have similar "bar" counts?
            }
            else {
                rate = followLeaderLength(myFrames, props.frames);
                goalFrames = props.frames;
            }
        }
    }
}

/**
 * Attempt to find a suitable location to start if we're following something.
 * Here after a change is made in THIS loop that requires that we re-orient with
 * the leaader.
 *
 * The ignoreCurrent flag passed to the inner followLeaderLocation is true to indiciate
 * that we have not been following something, or following something else, and
 * our current location is not meangingful.
 */
void MidiTrack::followLeaderLocation()
{
    // ignore if we're empty
    int myFrames = recorder.getFrames();
    if (myFrames > 0) {

        // ignore if we don't have an active leader track
        // !! not enough for host/midi leaders
        int leaderTrack = scheduler.findLeaderTrack();
        if (leaderTrack > 0) {
            TrackProperties props;
            manager->getTrackProperties(leaderTrack, props);
            if (props.invalid) {
                Trace(1, "MidiTrack: followLeaderSize() was given an invalid audio track number %d", leaderTrack);
            }
            else if (props.frames == 0) {
                // leader is empty, just continue with what we have now
            }
            else if (myFrames != props.frames) {
                int startFrame = followLeaderLocation(myFrames, recorder.getFrame(),
                                                      props.frames, props.currentFrame,
                                                      rate, true, true);

                recorder.setFrame(startFrame);
                player.setFrame(startFrame);
            }
        }
    }
}

/**
 * Here after we have made a fundamental change to this loop and need to
 * consider what happens when we're following another loop.
 */
void MidiTrack::reorientFollower(int previousFrames, int previousFrame)
{
    // what was the purpose of this?
    // followLeaderLocation() would ignore it anyway
    // if we recalculate leader follow frame every time, and we didn't
    // change size, we should end up back at the same point if we were already
    // following so trying to preserve the prevous frame isn't really necessary
    (void)previousFrame;
    
    // ignore if we don't have an active leader track
    // !! not enough for host/midi leaders
    int leaderTrack = scheduler.findLeaderTrack();
    if (leaderTrack > 0) {
        TrackProperties props;
        manager->getTrackProperties(leaderTrack, props);
        if (props.invalid) {
            Trace(1, "MidiTrack: followLeaderSize() was given an invalid audio track number %d", leaderTrack);
        }
        else if (props.frames == 0) {
            // leader is empty, just continue with what we have now
        }
        else if (previousFrames == recorder.getFrames()) {
            // we went somewhere that was the same size as the last time
            // don't need to resize, but may need to change location
            followLeaderLocation();
        }
        else {
            // all bets are off, do both
            followLeaderSize();
            followLeaderLocation();
        }
    }
}

/**
 * todo: this is obsolete, but keep it around for awhile if useful
 * 
 * Eventually called after a long process from a ClipStart event scheduled
 * in an audio track.
 *
 * This is kind of like an action, but TrackScheduler is not involved.
 * We've quantized to a location in the audio track and need to begin
 * playing now.
 *
 * The Track must be in a quiet state, e.g. no pending recording.
 */
void MidiTrack::clipStart(int audioTrack, int newIndex)
{
    if (recorder.hasChanges()) {
        // could try to unwind gracefully, but ugly if there is a rounding mode
        Trace(1, "MidiTrack: Unable to trigger clip in track with pending changes");
    }
    else {
        // we could have just passed all this shit up from where it came from
        TrackProperties props;
        manager->getTrackProperties(audioTrack, props);
        if (props.invalid) {
            Trace(1, "MidiTrack: clipStart was given an invalid audio track number %d", audioTrack);
        }
        else {
            // make the given loop active
            // this is very similar to finishSwitch except we don't do the EmptyLoopAction
            // if the desired clip loop is empty, it is probably a bad action
            MidiLoop* loop = loops[newIndex];
            if (loop == nullptr) {
                Trace(1, "MidiTrack: clipStart bad loop index %d", newIndex);
            }
            else if (loop->getFrames() == 0 || loop->getPlayLayer() == nullptr) {
                Trace(1, "MidiTrack: clipStart empty loop %d", newIndex);
            }
            else {
                // loop switch "lite"
                loopIndex = newIndex;
                MidiLayer* playing = loop->getPlayLayer();

                recorder.resume(playing);
                player.change(playing, 0);
                // ambiguity over minor modes, but definitely turn this off
                overdub = false;

                // now that we've got the right loop in place,
                // resize and position it as if the Resize command
                // had been actioned on this track
                leaderResized(props);
                mode = TrackState::ModePlay;

                // player was usually in pause
                player.setPause(false, true);

                // I don't think we have TrackScheduler issues at this point
                // we can only get a clipStart event from an audio track,
                // and audio tracks are advanced before MIDI tracks
                // so we'll be at the beginning of the block at this point
                scheduler.setFollowTrack(props.number);
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
