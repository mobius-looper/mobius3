
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/SessionConstants.h"
#include "../../model/Session.h"

#include "../track/TrackManager.h"
#include "../track/LogicalTrack.h"
#include "../track/MslTrack.h"
#include "../track/TrackProperties.h"

#include "Transport.h"
#include "HostAnalyzer.h"
#include "MidiAnalyzer.h"
#include "BarTender.h"
#include "SyncMaster.h"

#include "Unitarian.h"

Unitarian::Unitarian(SyncMaster* sm)
{
    syncMaster = sm;
    barTender = sm->getBarTender();
}

Unitarian::~Unitarian()
{
}

/**
 * Return the unit length for one of the SyncSources.
 * This may not be used for SyncSourceTrack.
 * The result is zero for None and Master since Master is variable
 * and self defining.
 * 
 * Zero is possible for MidiAnalyzer if we're before the first beat,
 * so this can't be used for an "am I synced" test.
 */
int Unitarian::getUnitLength(SyncSource src)
{
    int length = 0;
    switch (src) {
        case SyncSourceNone:
        case SyncSourceMaster:
            break;
        case SyncSourceTrack:
            Trace(1, "Unitarian::getUnitLength(SyncSource) with SyncSourceTrack");
            break;
        case SyncSourceMidi:
            length = syncMaster->getMidiAnalyzer()->getUnitLength();
            break;
        case SyncSourceHost:
            length = syncMaster->getHostAnalyzer()->getUnitLength();
            break;
        case SyncSourceTransport:
            length = syncMaster->getTransport()->getUnitLength();
            break;
    }
    return length;
}

//////////////////////////////////////////////////////////////////////
//
// TrackSync Units
//
// These are less well defined than the external sync sources.
// Subcycles often correspond to "beats" and cycles to "bars" but
// it depends on how the loop was recorded and edited.
//
// The TracSyncMaster is often recorded against another external sync
// source in which case you could consider the base unit to be the size
// of unit the track was recorded with rather than soem arbitrary subdivision
// of the track itself.
//
// For the purposes of AutoRecord which is where this is primarily used,
// we consider Beat=Subcycle and Bar=Cycle.
//
//////////////////////////////////////////////////////////////////////

/**
 * Return one of the unit lengths of this track.
 *
 * Two ways evolved to do this.  They should be the same
 * but the way they go about it is different.  Need to verify this.
 * Currently this is used only for verification of the final record length
 * and preliminary sizing for the UI.  The actual synced recording will be
 * pulsed.
 *
 * When there is an odd number of cycles or subcycles, the final cycle/subcycle
 * can be of a different size than the others due to roundoff.  e.g.
 * If a track is 100 frames long and has 3 cycles, the first two will be 33 and
 * the last one will be 34.  Depending on which cycles were included in the
 * recording this may fail verification.
 *
 * This won't happen all the time with odd numbers, tracks usually start out with
 * one cycle and multiply from there, and all cycles will be the same length.
 * But it can happen if the user arbitrarily changes the cycle count after recording.
 * 
 * Subcycles are more problematic.  A track always starts with a single cycle but
 * if you wanted the time signature to be 5/4 with subcycles=5, then for a 128 frame
 * cycle the first 4 subcycles would be 25 and the last one would be 28.
 * This anomoly repeats on every cycle.  So depending on which subcycle the recording
 * starts and ends on there can be several different outcomes.
 *
 * The error is small enough that it will cause minimal drift but it can't be prevent
 * unless you start doing complicated maintenance of fractional lengths, or periodic
 * corrections.  And it only happens if you're using odd numbers of things which is rare.
 *
 * If the track is empty zero is returned, and this is not logged as an error.
 */
int Unitarian::getTrackUnitLength(LogicalTrack* track, TrackSyncUnit unit)
{
    int length = 0;
    
    // method 1: TrackProperties with simple division
    TrackProperties props;
    track->getTrackProperties(props);

    if (props.frames > 0) {
        if (unit == TrackUnitLoop) {
            length = props.frames;
        }
        else {
            int cycles = props.cycles;
            if (cycles == 0) {
                // this is not supposed to happen, assume 1
                Trace(1, "Unitarian::getTrackUnitLength Track had no cycles");
                cycles = 1;
            }

            if (unit == TrackUnitCycle)
              length = (props.frames / cycles);

            else if (unit == TrackUnitSubcycle) {
                int subcycles = props.subcycles;
                if (subcycles == 0) {
                    // also not supposed to happen
                    // 4 is the most common, but be consistent with 1
                    Trace(1, "Unitarian::getTrackUnitLength Track had no subcycles");
                    subcycles = 1;
                }

                length = (props.frames / cycles) / subcycles;
            }
            else {
                // TrackSyncUnitNoen exists for configuration but
                // should not be used at this point
            }
        }
    }
    return length;
}

/**
 * Method 2 for determining the track unit lengths.
 * This goes through the same process that MSL uses and relies
 * on the BaseTrack implementation to figure it out.
 * Dislike the duplication.
 */
int Unitarian::getTrackUnitLength2(LogicalTrack* lt, TrackSyncUnit unit)
{
    int length = 0;
    
    MslTrack* innerTrack = lt->getMslTrack();
    if (innerTrack == nullptr) {
        Trace(1, "Unitarian::getTrackUnitLength What the hell is this thing?");
    }
    else {
        switch (unit) {
            case TrackUnitLoop:
                length = innerTrack->getFrames();
                break;
            case TrackUnitCycle:
                length = innerTrack->getCycleFrames();
                break;
            case TrackUnitSubcycle:
                length = innerTrack->getSubcycleFrames();
                break;
        }
    }
    return length;
}

/**
 * Return the track unit length for the leader track of the given follower.
 *
 * This may return zero if the leader is empty, or there is no TrackSyncMaster
 * or if the follower does not use SyncSourceTrack.
 */
int Unitarian::getLeaderUnitLength(LogicalTrack* follower, TrackSyncUnit unit)
{
    int length = 0;
    // jebus, really need to move all the unit shit down here
    LogicalTrack* leader = syncMaster->getLeaderTrack(follower);
    if (leader != nullptr) 
      length = getTrackUnitLength(leader, unit);
    return length;
}

//////////////////////////////////////////////////////////////////////
//
// AutoRecord
//
//////////////////////////////////////////////////////////////////////

/**
 * Return the number of frames in one AutoRecord "unit".
 *
 * This includes both external sync units and Track units which
 * are more ambiguous.
 *
 * For external sources this is a multiple of the "base" unit which
 * represents one Beat.
 *
 * For track sources this will be one of the subdivisions subcycle,
 * cycle or loop.
 *
 * A special case exists when the SyncSource is None.
 * Here the length of the AR is defined by the Transport tempo and the SyncUnit
 * from the session.  This is the only time where SyncUnit is relevant when
 * SyncSource is None.
 *
 * NOTE: There is an older parameter named autoRecordUnit that was intended
 * to define which unit (beat/bar/loop) to use for AR.  I decided not to use
 * this since it overlaps with the syncUnit parameter used for synchronized
 * recording and they're almost always the same.  Take it out unless you
 * find a need.
 *
 * todo: Move this to BarTender
 */
int Unitarian::getSingleAutoRecordUnitLength(LogicalTrack* track)
{
    int recordLength = 0;
    SyncSource src = syncMaster->getEffectiveSource(track);
    SyncUnit unit = track->getSyncUnitNow();

    if (src == SyncSourceTrack) {
        // tracks do not have beat-based units
        // convert the SyncUnit to a TrackSyncUnit and get the length of that
        // the enum conversion is annoying but works if you're careful
        TrackSyncUnit tsu = (TrackSyncUnit)unit;
        recordLength = getLeaderUnitLength(track, tsu);
    }
    else {
        // these all have beat-based units
        SyncSource beatSource = src;
        if (beatSource == SyncSourceNone) {
            // for the purposes of auto-record, we need to get
            // a tempo from somewhere, OG Mobius had autoRecordTempo
            // until we see a need for something more, let the Transport
            // define this
            beatSource = SyncSourceTransport;
        }
        else if (beatSource == SyncSourceMaster) {
            // getEffectriveSource should have mapped this already
            Trace(1, "SyncMaster: Confusion finding AutoRecord unit length for Master");
            beatSource = SyncSourceTransport;
        }
        
        int beatLength = 0;
        switch (beatSource) {
            case SyncSourceTransport:
                beatLength = syncMaster->getTransport()->getUnitLength();
                break;
            case SyncSourceHost:
                beatLength = syncMaster->getHostAnalyzer()->getUnitLength();
                break;
            case SyncSourceMidi:
                beatLength = syncMaster->getMidiAnalyzer()->getUnitLength();
                break;
            case SyncSourceNone:
            case SyncSourceMaster:
            case SyncSourceTrack:
                // handled in the other clauses
                break;
        }

        // if the SyncUnit is bar or loop then the beat unit length
        // is multipled by whatever the beatsPerBar for that source is
        if (unit == SyncUnitBar || unit == SyncUnitLoop) {
            int barMultiplier = barTender->getBeatsPerBar(beatSource);
            recordLength = beatLength * barMultiplier;
        }

        // if the syncUnit is Loop, then one more multiple
        if (unit == SyncUnitLoop) {
            int loopMultiplier = barTender->getBarsPerLoop(beatSource);
            recordLength *= loopMultiplier;
        }
    }
    
    return recordLength;
}

/**
 * Get the fundamental unit length for locking a track to
 * an external sync source.  There is logic in here to handle TrackSync
 * too for completeness but we don't do drift correction for track sync.
 * 
 * Shares DNA with AutoRecord.
 */
int Unitarian::getLockUnitLength(LogicalTrack* track)
{
    int lockLength = 0;
    SyncSource src = syncMaster->getEffectiveSource(track);
    SyncUnit unit = track->getSyncUnitNow();

    if (src == SyncSourceTrack) {
        // tracks do not have beat-based units
        // convert the SyncUnit to a TrackSyncUnit and get the length of that
        // the enum conversion is annoying but works if you're careful

        // !! for the purpose of the locked unit, it might be better
        // if we standardized on the cycle length?
        // this won't work for auto record then...
        TrackSyncUnit tsu = (TrackSyncUnit)unit;
        lockLength = getLeaderUnitLength(track, tsu);
    }
    else {
        // these all have beat-based units
        SyncSource beatSource = src;
        if (beatSource == SyncSourceNone) {
            // for the purposes of auto-record, we need to get
            // a tempo from somewhere, OG Mobius had autoRecordTempo
            // until we see a need for something more, let the Transport
            // define this
            beatSource = SyncSourceTransport;
        }
        else if (beatSource == SyncSourceMaster) {
            // getEffectriveSource should have mapped this already
            Trace(1, "SyncMaster: Confusion finding AutoRecord unit length for Master");
            beatSource = SyncSourceTransport;
        }
        
        int beatLength = 0;
        switch (beatSource) {
            case SyncSourceTransport:
                beatLength = syncMaster->getTransport()->getUnitLength();
                break;
            case SyncSourceHost:
                beatLength = syncMaster->getHostAnalyzer()->getUnitLength();
                break;
            case SyncSourceMidi:
                beatLength = syncMaster->getMidiAnalyzer()->getUnitLength();
                break;
            case SyncSourceNone:
            case SyncSourceMaster:
            case SyncSourceTrack:
                // handled in the other clauses
                break;
        }

        lockLength = beatLength;
    }
    
    return lockLength;
}

//////////////////////////////////////////////////////////////////////
//
// Verfification
//
//////////////////////////////////////////////////////////////////////

/**
 * Immediately after recording, verify that the track has a length that
 * is compatible with it's sync source.
 */
void Unitarian::verifySyncLength(LogicalTrack* lt)
{
    Trace(2, "SyncMaster: Sync recording ended with %d frames",
          lt->getSyncLength());
    
    // tehnically we should store the SyncSource that was used when the
    // recording first began, not whatever it is now, unlikely to change
    // DURING recording, but it could change after the track is allowed
    // to live for awhile
    SyncSource src = syncMaster->getEffectiveSource(lt);
    int trackLength = lt->getSyncLength();

    if (src == SyncSourceTrack) {
        // this one is harder...cycles should divide cleanly but
        // subcycles won't necessarily if there was an odd number

        LogicalTrack* leader = syncMaster->getLeaderTrack(lt);
        if (leader == nullptr) {
            Trace(1, "SyncMaster::verifySyncLength No leader track");
        }
        else {
            SyncUnit unit = lt->getSyncUnitNow();
            // live dangerously
            TrackSyncUnit tsu = (TrackSyncUnit)unit;
            int leaderUnit = getTrackUnitLength(leader, tsu);

            if (leaderUnit == 0)
              Trace(1, "SyncMaster: Unable to get base unit length for Track Sync");
            else {
                int leftover = trackLength % leaderUnit;
                if (leftover != 0)
                  Trace(1, "SyncMaster: TrackSync recording leftovers %d", leftover);

                leftover = leader->getSyncLength() % leaderUnit;
                if (leftover != 0)
                  Trace(1, "SyncMaster: TrackSync master leftovers %d", leftover);
            }
        }
    }
    else if (src == SyncSourceMidi) {
        MidiAnalyzer* midiAnalyzer = syncMaster->getMidiAnalyzer();
        // this one is more complicated
        // similiar verification but if we end outside the unitLength
        // and there are no other followers we can try to override it 
        if (!midiAnalyzer->isLocked())
          Trace(1, "SyncMaster: MidiAnalyzer was not locked after recording ended");
                
        int unit = midiAnalyzer->getUnitLength();
        if (unit == 0) {
            // this is the "first beat recording" fringe case
            // the end should have been pulsed and remembered
            Trace(1, "SyncMaster: Expected MIDI to know what was going on by now");
        }
        else {
            int leftover = trackLength % unit;
            if (leftover != 0) {
                Trace(1, "SyncMaster: MIDI sync recording verification failed: leftovers %d", leftover);
                int others = getActiveFollowers(SyncSourceMidi, unit, lt);
                if (others != 0) {
                    Trace(1, "SyncMaster: Unable to relock unit length for abnormal track");
                }
                else {
                    // yet another unit calculator
                    int trackUnitLength = calculateUnitLength(lt);
                    if (!midiAnalyzer->forceUnitLength(trackUnitLength))
                      Trace(1, "SyncMaster: Unable to relock unit length for abnormal track");
                    lt->setUnitLength(trackUnitLength);
                }
            }
        }
    }
    else {
        // these don't jitter and should always swork
        int baseUnit = getUnitLength(src);
        if (baseUnit > 0) {
            int leftover = trackLength % baseUnit;
            if (leftover != 0)
              Trace(1, "SyncMaster: Sync recording verification failed: leftovers %d", leftover);
        }
    }
}

/**
 * Given a track that may have been recording with unstable pulse widths
 * calculate the actual unit length.
 * In practice this happens only when syncing to MIDI with an unlocked analyzer.
 *
 * This assumes the track was a normal synchronized recording of some number of "bars"
 * from the sync source and that the unit length is the bar/cycle length divided
 * by the source beatsPerBar.  If this is a randomly recorded track this may not fit well.
 */
int Unitarian::calculateUnitLength(LogicalTrack* lt)
{
    int unitLength = 0;

    TrackProperties props;
    lt->getTrackProperties(props);

    if (props.invalid) {
        Trace(1, "SyncMaster::calculateUnitLength Unable to get TrackProperties");
    }
    else {
        // should we trust the track's cycle count?  Should still valid and the
        // same as goal units
        if (props.cycles != lt->getSyncGoalUnits()) {
            Trace(1, "SyncMaster: Goal unit/cycle mismatch %d %d",
                  lt->getSyncGoalUnits(), props.cycles);
        }

        if (props.cycles == 0) {
            Trace(1, "SyncMaster::calculateUnitLength Cycle count was zero");
        }
        else {
            int cycleLength = props.frames / props.cycles;
            int bpb = barTender->getBeatsPerBar(lt->getSyncUnitNow());
            unitLength = cycleLength / bpb;

            if (cycleLength % bpb)
              Trace(1, "SyncMaster::calculateUnitLength Uneven unitLength, cycle %d bpb %d",
                    cycleLength, bpb);
        }
    }
    return unitLength;
}

/**
 * A follower is "active" it it uses this sync source and it is not empty (in reset).
 * This is called only by MidiAnalyzer ATM to know whether it is safe to make continuous
 * adjustments to the locked unit length or whether it needs to retain the current unit
 * length and do drift notifications.
 *
 * Once fully recorded, a follower is only active if it was recorded with the same unit
 * length that is active now.  This allows the following to be broken after the user deliberately
 * changes the device tempo, forcing a unit recalculation which is then used for new recordings.
 */
int Unitarian::getActiveFollowers(SyncSource src, int unitLength, LogicalTrack* notThisOne)
{
    int followers = 0;
    TrackManager* trackManager = syncMaster->getTrackManager();
    
    for (int i = 0 ; i < trackManager->getTrackCount() ; i++) {
        LogicalTrack* lt = trackManager->getLogicalTrack(i+1);

        if (lt == notThisOne) continue;

        // !! this either needs to be getEffectiveSource but really once
        // a track has been recorded we need to save the source it was
        // recorded with along with the unit length, it won't normally matter for
        // Midi or Host but Master is weird.  It doesn't really matter though, if the
        // source changes and it just happens to have the right unit length it will
        // effectively assimilate as a follower
        if (lt->getSyncSourceNow() == src) {
            // todo: still some lingering issues if the track has multiple loops
            // and they were recorded with different unit lenghts, that would be unusual
            // but is possible

            int trackUnitLength = lt->getUnitLength();
            // not saving this on every loop, see if a disconnect happened
            int syncLength = lt->getSyncLength();
            if (syncLength > 0) {
                int leftover = syncLength % unitLength;
                if (leftover > 0) {
                    // within the track itself this needs to match
                    // !! this means if you do an unrounded multiply or other form of
                    // edit that randomly changes the length, it needs to clear the
                    // unitLength so we don't bitch about it here
                    Trace(1, "SyncMaster: Track length doesn't match unit length %d %d",
                          syncLength, unitLength);
                }
            }
            
            if (trackUnitLength == unitLength)
              followers++;
        }
    }
    return followers;
}

int Unitarian::getActiveFollowers(SyncSource src, int unitLength)
{
    return getActiveFollowers(src, unitLength, nullptr);
}
    
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


