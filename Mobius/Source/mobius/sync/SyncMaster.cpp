
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/Symbol.h"
#include "../../model/Session.h"
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../../model/SystemState.h"
#include "../../model/SyncState.h"
#include "../../model/TrackState.h"
#include "../../model/PriorityState.h"

// for some of the old sync related modes
#include "../../model/ParameterConstants.h"

#include "../MobiusKernel.h"
#include "../track/TrackManager.h"
#include "../track/LogicalTrack.h"
#include "../track/MslTrack.h"
#include "../track/TrackProperties.h"

#include "Pulsator.h"
#include "Pulse.h"
#include "MidiRealizer.h"
#include "MidiAnalyzer.h"
#include "HostAnalyzer.h"
#include "Transport.h"
#include "TimeSlicer.h"

#include "SyncMaster.h"

SyncMaster::SyncMaster()
{
}

SyncMaster::~SyncMaster()
{
}

/**
 * The MobiusContainer is necessary for these things:
 *   - access to MidiManager
 *   - access to sendAlert()
 *   - access to the sampleRate
 *
 * MidiAnalyzer needs MidiManager to register the realtime event listener.
 * MidiRealizer needs MidiManager to send clock events.
 *
 * MidiRealizer uses sendAlert() to send a few warning messages to the user related
 * to midi device configuration.
 *
 * MidiRealizer and Transport use the SampleRate for some timing calculations.
 *
 * In time, try to factor out a more focused SyncContainer that hides all the other
 * dependencies MobiusContainer drags in.  The MidiRealtimeListener will be a problem
 * since that would have to move out of MidiManager.
 */
void SyncMaster::initialize(MobiusKernel* k, TrackManager* tm)
{
    kernel = k;
    trackManager = tm;
    container = kernel->getContainer();
    sessionHelper.setSymbols(container->getSymbols());

    // these are now dynamically allocated to reduce header file dependencies
    midiRealizer.reset(new MidiRealizer());
    midiAnalyzer.reset(new MidiAnalyzer());
    hostAnalyzer.reset(new HostAnalyzer());
    transport.reset(new Transport(this));

    barTender.reset(new BarTender(this, trackManager));
    pulsator.reset(new Pulsator(this, trackManager, barTender.get()));

    // reach out and touch the face of god
    hostAnalyzer->initialize(container->getAudioProcessor());

    MidiManager* mm = container->getMidiManager();
    midiRealizer->initialize(this, mm);
    midiAnalyzer->initialize(this, mm);

    timeSlicer.reset(new TimeSlicer(this, trackManager));

    // start everything off with a default sample rate, but this
    // may change as soon as the audio devices are open
    refreshSampleRate(44100);
}

/**
 * Pulsator needs this for a few things
 * Transport should be using it for the starting tempo, and various options.
 */
void SyncMaster::loadSession(Session* s)
{
    barTender->loadSession(s);
    pulsator->loadSession(s);
    transport->loadSession(s);
    timeSlicer->loadSession(s);

    // cached parameters
    manualStart = sessionHelper.getBool(s, ParamTransportManualStart);  
    autoRecordUnits = sessionHelper.getInt(s, ParamAutoRecordUnits);
    recordThreshold = sessionHelper.getInt(s, ParamRecordThreshold);
}

/**
 * Called during the Supervisor::shutdown process.  Make sure the
 * clock generator thread is cleanly stopped.
 */
void SyncMaster::shutdown()
{
    midiRealizer->shutdown();
    midiAnalyzer->shutdown();
}

/**
 * Here when a FuncGlobalReset action is intercepted.
 */
void SyncMaster::globalReset()
{
    transport->globalReset();
    barTender->globalReset();
    midiAnalyzer->globalReset();

    // host analyzer doesn't reset, it continues monitoring the host
    // same with MIDI
}

/**
 * Needed by BarTender, and eventually TimeSlicer if you move it under here.
 */
TrackManager* SyncMaster::getTrackManager()
{
    return kernel->getTrackManager();
}

/**
 * Needed by MidiAnalyzer so it can pull things from the Session
 */
SymbolTable* SyncMaster::getSymbols()
{
    return kernel->getContainer()->getSymbols();
}

int SyncMaster::getRecordThreshold()
{
    return recordThreshold;
}

//////////////////////////////////////////////////////////////////////
//
// Synchronized Recording
//
//////////////////////////////////////////////////////////////////////

/**
 * Track wants to know this when scheduling AutoRecord stop
 */
bool SyncMaster::isSyncRecording(int number)
{
    bool syncing = false;
    LogicalTrack* t = trackManager->getLogicalTrack(number);
    if (t != nullptr) {
        syncing = t->isSyncRecording();
    }
    return syncing;
}

/**
 * This has historically only returned true if the track was not synchronizaing.
 * If you're synchronizing, waiting for a threshold is much less useful since
 * you know when it's going to start and have time to prepare.
 *
 * Still, they could potentially be combined.  The threshold would be necessary
 * to start the process which may then suspend waiting for a sync pulse.  But this
 * 
 * todo: While threshold is useful on the recording of the first loop, it should
 * be disabled for EmptyLoopAction=Record and some other things.
 */
bool SyncMaster::hasRecordThreshold(int number)
{
	bool threshold = false;
    if (recordThreshold > 0) {
        threshold = !isRecordSynchronized(number);
	}
    return threshold;
}

SyncSource SyncMaster::getEffectiveSource(int id)
{
    SyncSource source = SyncSourceNone;
    LogicalTrack* t = trackManager->getLogicalTrack(id);
    if (t != nullptr) {
        source = getEffectiveSource(t);
    }
    return source;
}
        
/**
 * Get the effective sync source for a track.
 *
 * There are two complications here:  
 * The complication here is around SourceMaster which is only allowed
 * if there is no other sync master.
 */
SyncSource SyncMaster::getEffectiveSource(LogicalTrack* lt)
{
    SyncSource source = SyncSourceNone;
    if (lt != nullptr) {
        source = lt->getSyncSourceNow();
        if (source == SyncSourceMaster) {
            int transportMaster = transport->getMaster();
            if (transportMaster > 0 && transportMaster != lt->getNumber()) {
                // there is already a transport master, this track
                // reverts to following the transport
                // !! here is where we need an option to fall back to SyncSourceTrack
                // like we used to
                source = SyncSourceTransport;
            }
        }
        else if (source == SyncSourceTrack) {
            // this is relevant only if there is a track sync master
            source = SyncSourceNone;
            if (trackSyncMaster > 0) {
                // yes, but only if it has content?
                // I suppose this could just start waiting until you record something
                // into the current master track but this raises complications...
                LogicalTrack* mlt = trackManager->getLogicalTrack(trackSyncMaster);
                if (mlt != nullptr && mlt != lt)
                  source = SyncSourceTrack;
            }
        }
    }
    return source;
}

SyncUnit SyncMaster::getSyncUnit(int id)
{
    SyncUnit unit = SyncUnitBeat;
    LogicalTrack* t = trackManager->getLogicalTrack(id);
    if (t != nullptr) {
        unit = t->getSyncUnitNow();
    }
    return unit;
}

/**
 * Returns true if the start/stop of a recording is synchronized.
 * If this returns true, it will usually be followed immediately
 * by a call to requestRecordStart or requestRecordStop and it is
 * expected that those succeed.  Not liking the dependency, but
 * works well enough.
 */
bool SyncMaster::isRecordSynchronized(int number)
{
    bool sync = false;
    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {
        SyncSource src = getEffectiveSource(lt);
        sync = (src != SyncSourceNone && src != SyncSourceMaster);
    }
    return sync;
}

/**
 * Called by the track in response to an action to begin the
 * recording process.   This interface provides the most flexibility
 * to control the recording pulses.  Other signatures derive the arguments
 * from session parameters.
 *
 * This does not allow overriding the track's SyncMode from the session.
 * I suppose it could, but it's easy enough to do it with scripts.  The two
 * units may be specified in a script or as action arguments.
 *
 * If the result has the synchronized flag set, the track is expected to
 * schedule an internal event that will be activated on the next startUnit pulse.
 *  
 * The ending of the recording will be quantized to the pulseUnit.
 * xxx
 * a SyncEvent will be sent on each , pulseUnit pulses will be sent to the track
 * to do things like increment cycle counts or other state related to the increasing
 * length of the loop.
 *
 * The recording process may be ended at any time by the track calling
 * requestRecordStop or by the return value of any syncPulse as pulses are
 * sent into the track.
 *
 * todo: reconsider the need for an alternate startUnit
 */
SyncMaster::RequestResult SyncMaster::requestRecordStart(int number,
                                                         SyncUnit recordUnit,
                                                         SyncUnit startUnit,
                                                         bool noSync)
{
    RequestResult result;

    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {
        SyncSource src = getEffectiveSource(lt);
        
        if (src == SyncSourceNone || src == SyncSourceMaster || noSync) {
            // return an empty result and let the track sort it out
        }
        else {
            result.synchronized = true;

            SyncUnit defaultUnit = lt->getSyncUnitNow();
            if (defaultUnit == SyncUnitNone) {
                Trace(1, "SyncMaster: Someone stored SyncUnitNone in the session");
                defaultUnit = SyncUnitBar;
            }

            if (recordUnit == SyncUnitNone)
              recordUnit = defaultUnit;

            if (startUnit == SyncUnitNone)
              startUnit = recordUnit;
            
            lt->setSyncRecording(true);
            lt->setSyncRecordUnit(recordUnit);
            lt->setSyncStartUnit(startUnit);
            lt->setUnitLength(barTender->getSourceUnitLength(src));
            // do NOT set Goal units here, we don't know what it will be yet
        }
    }
    return result;
}

/**
 * Used when the start and pulse units are the same.
 */
SyncMaster::RequestResult SyncMaster::requestRecordStart(int number,
                                                         SyncUnit unit, bool noSync)
{
    return requestRecordStart(number, unit, unit, noSync);
}

/**
 * Used when the start and pulse units come from session parameters.
 */
SyncMaster::RequestResult SyncMaster::requestRecordStart(int number, bool noSync)
{
    return requestRecordStart(number, SyncUnitNone, SyncUnitNone, noSync);
}

/**
 * This is called when a track responds to an action that triggers
 * the ending of the recording.  The recording normally ends
 * on the next sync pulse whose unit was defined in requestRecordStart.
 * 
 * It is expected to have called isRecordSynced first, or be able to deal
 * with this returning a Result that says it isn't synchronized.
 *
 * The important thing this does is lock the SyncAnalyzer, which in practice
 * is only important for MidiAnalyzer if we allowed the recording to start
 * during the warmup period.
 */
SyncMaster::RequestResult SyncMaster::requestRecordStop(int number, bool noSync)
{
    RequestResult result;
    
    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {
        
        SyncSource src = getEffectiveSource(lt);
        if (src == SyncSourceNone || src == SyncSourceMaster || noSync) {
            // return an empty result and let the track figure it out
        }
        else {
            // do deferred unit locking if not already locked
            // the only one that really needs this is MIDI, but go through the motions
            switch (src) {
                case SyncSourceMidi:
                    midiAnalyzer->lock();
                    break;
                case SyncSourceHost:
                    hostAnalyzer->lock();
                    break;
                case SyncSourceTransport:
                    transport->lock();
                    break;
                default: break;
            }
                    
            result.synchronized = true;

            // this is what switches us from sending Extend events
            // to sending the final Stop event
            if (lt->getSyncGoalUnits() > 0) {
                // why would this happen?
                Trace(1, "SyncMaster: Requested RecordStop with existing goal units");
            }
            else {
                int goal = lt->getSyncElapsedUnits();
                if (goal == 0) {
                    // we must be in the start synchronization period, requesting
                    // a stop in there becomes like an AutoRecord of one unit
                    Trace(2, "SyncMaster: AutoRecord conversion");
                    goal = 1;
                }
                lt->setSyncGoalUnits(goal);
            }
        }
    }

    // !! more to do here
    // if you're doing a manual recording ending with MIDI
    // after locking it will end up with a unit length that may not be compatible
    // with the length of the recorded loop so far, this is usually the final pulse
    // so it needs to be adjusted like we do for AR or converted
    // to a SyncEvent::Finalize
    
    return result;
}

/**
 * Variant for AutoRecord
 * A bounded recording is being requested so SM knows when it is supposed to end.
 * Not supporting sync unit overrides here yet, that concept needs more thought.
 */
SyncMaster::RequestResult SyncMaster::requestAutoRecord(int number, bool noSync)
{
    RequestResult result;

    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {
        
        if (lt->isSyncRecording())
          Trace(1, "SyncMaster: Request to start AutoRecord while already in a recording");
        lt->resetSyncState();
        
        result.autoRecordUnits = getAutoRecordUnits(lt);
        int unitLength = getAutoRecordUnitLength(lt);
        // supply this even if we're syncing to give the loop some girth during
        // we can see in the LoopMeter during the initial recording
        result.autoRecordLength = unitLength * result.autoRecordUnits;

        lt->setSyncGoalUnits(result.autoRecordUnits);
        
        SyncSource src = getEffectiveSource(lt);

        if (src != SyncSourceNone && src != SyncSourceMaster &&  !noSync) {

            result.synchronized = true;

            SyncUnit syncUnit = lt->getSyncUnitNow();
            if (syncUnit == SyncUnitNone) {
                Trace(1, "SyncMaster: Someone stored SyncUnitNone in the session");
                syncUnit = SyncUnitBar;
            }
            
            lt->setSyncRecording(true);
            lt->setSyncStartUnit(syncUnit);
            lt->setSyncRecordUnit(syncUnit);

            lt->setSyncElapsedUnits(1);
        }

        // in both cases, let it know if there is a recording threshold
        // old comments wonder if the noSync option on the Action should
        // disable threshold recording
        // threshold has historically been disabled if we're synchronizing
        // I imagine they could be combined but it gets messy and complicates testing
        if (!noSync && !result.synchronized)
          result.threshold = recordThreshold;
    }
    return result;
}

/**
 * Used when we're stuck in Synchronize or Threshold modes at
 * the beginning of a recording and they press Record again.
 * Similar to an AutoRecord of one unit.
 */
SyncMaster::RequestResult SyncMaster::requestPreRecordStop(int number)
{
    RequestResult result;

    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {
        // whether synced or unsynced return the length
        result.autoRecordUnits = 1;
        result.autoRecordLength = getAutoRecordUnitLength(lt);
        lt->setSyncGoalUnits(1);

        SyncSource src = getEffectiveSource(lt);
        if (src != SyncSourceNone && src != SyncSourceMaster) {
            result.synchronized = true;
        }
    }
    return result;
}

/**
 * Return the number of units as returned by getAutoRecordUnitLength
 * are to be included in an AutoRecord.
 *
 * This is defined by another parameter.  The two are normally multiplied
 * to gether to get the total length with the recordUnits number becomming
 * the number of cycles in the loop.
 */
int SyncMaster::getAutoRecordUnits(LogicalTrack* track)
{
    // this one is not senstiive to the syncSource
    (void)track;
    int units = autoRecordUnits;
    if (units <= 0) units = 1;
    return units;
}

/**
 * Return the number of frames in one AutoRecord "unit".
 * This is defined by the base(beat) unit length in the track's
 * sync source combined with the syncUnit parameter which multiplies
 * the number of beats.
 *
 * A special case exists when the SyncSource is None.
 * Here the length of the AR is defined by the Transport tempo and the syncUnit
 * from the session.  This is the only time where SyncUnit is relevant when
 * SyncSource is None.  
 */
int SyncMaster::getAutoRecordUnitLength(LogicalTrack* track)
{
    int length = barTender->getSourceUnitLength(track);

    // if the syncUnit is bar or loop then the beat unit length
    // is multipled by whatever the beatsPerBar for that source is
    SyncUnit unit = track->getSyncUnitNow();
    if (unit == SyncUnitBar || unit == SyncUnitLoop) {
        int barMultiplier = barTender->getBeatsPerBar(track);
        length *= barMultiplier;
    }

    // if the syncUnit is Loop, then one more multiple
    if (unit == SyncUnitLoop) {
        int loopMultiplier = barTender->getBarsPerLoop(track);
        length *= loopMultiplier;
    }
    return length;
}

SyncMaster::RequestResult SyncMaster::requestExtension(int number)
{
    RequestResult result;
    
    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {
        // the number of units to extend, it will always be at least 1
        // for AutoRecord it could be the number of configured units, but
        // I'm thinking just keep it 1 for more fine control
        // if we wanted to have different extension amounts for AutoRecord
        // would need to pass in a flag since we don't know the function that
        // cause this here
        int extension = 1;

        int current = lt->getSyncGoalUnits();
        if (current == 0) {
            // this must be the first extension after ending a recording
            current = 1;
        }

        result.goalUnits = current + extension;
        lt->setSyncGoalUnits(result.goalUnits);

        // for unsynced recordings, calculate the length to add
        result.extensionLength = getAutoRecordUnitLength(lt);
    }
    return result;
}

/**
 * While you can always extend, reducing the goal units could
 * retroactively change the meaning of the last sync pulse if it
 * has already been processed in this block.  The conditions where this
 * could happen are very rare but possible:
 *
 *   - block contains a sync unit pulse
 *   - track advances to that pulse and treats it as an extension
 *   - track continues and resumes a script that causes the reduction (e.g. an Undo)
 *   - the last pulse should now be treated as a recording ending pulse
 *     rather than an extension
 *
 * This isn't something we can go back in time for, the script did logically happen
 * AFTER the extension so if you undo at this point the track either needs to ignore
 * it or undo the entire recording.
 *
 * If the reduction attempts to go behind the current recording location there are two schools
 * of though here: return 0 and expect the track to reset the loop.
 * Clamp it at 1 or elapsedUnits and just let the recording finish.
 *
 * Resetting the loop is more consistent with Undo immediately after Record,  but once
 * you've done AutoRecord and passed one elapsed unit, you're more in the zone of using
 * Undo/Redo to adjust the ending location and don't want to reset by accident.
 * Could have a parameter for this, but I'm liking just letting it finish.
 *
 */
SyncMaster::RequestResult SyncMaster::requestReduction(int number)
{
    RequestResult result;
    
    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {

        int reduction = 1;

        int current = lt->getSyncGoalUnits();
        if (current == 0) {
            // must be in the initial recording before the end frame was set
            // it is unexpected to call this
            current = 1;
        }
        
        int newUnits = current - reduction;
        int unitLength = getAutoRecordUnitLength(lt);

        // looking at getSyncElapsedUnits doesn't work for unsynced tracks
        // so do both synced and unsynced the same way by looking at their
        // record location
        int location = lt->getSyncLocation();
        int elapsed = (int)ceil((float)location / (float)unitLength);
        
        if (newUnits < elapsed) {
            Trace(2, "SyncMaster: Supressing attempt to reduce auto record before elapsed");
            newUnits = elapsed;
        }

        result.goalUnits = newUnits;
        lt->setSyncGoalUnits(newUnits);

        // for unsynced recordings, calculate the length to add
        result.extensionLength = unitLength;
    }
    return result;
}    


/**
 * Called by TimeSlicer to return a sync pulse for this track if one is
 * available from the track's SyncSource.
 *
 * Any pulse from this track's source is returned, the relevance of that is sorted
 * out later in handleBlockPulse.
 *
 * We can filter out noise by only returning pulses if the track is in an
 * active state of synchronized recording.
 */
Pulse* SyncMaster::getBlockPulse(LogicalTrack* track)
{
    Pulse* pulse = nullptr;
    if (track->isSyncRecording())
      pulse = pulsator->getAnyBlockPulse(track);
    return pulse;
}

/**
 * Called by TimeSlicer to handle the Pulse that we gave it with
 * getBlockPulse().  The track has been advanced up to this point
 * and we can now mess with it.
 */
void SyncMaster::handleBlockPulse(LogicalTrack* track, Pulse* pulse)
{
    if (!track->isSyncRecordStarted()) {
        // waiting for a start pulse
        Pulse* annotated = barTender->annotate(track, pulse);
        SyncUnit startUnit = track->getSyncStartUnit();
        if (startUnit == SyncUnitNone) {
            // should have stored these when we started all this
            Trace(1, "SyncMaster: Someone forgot to store their units");
            startUnit = track->getSyncUnitNow();
        }
        if (isRelevant(annotated, startUnit)) {
            sendSyncEvent(track, SyncEvent::Start);
        }
        // should be clear but make sure
        track->setSyncElapsedBeats(0);
        track->setSyncElapsedUnits(0);
        track->setSyncRecordStarted(true);
    }
    else {
        // always advance a beat
        int beat = track->getSyncElapsedBeats() + 1;
        track->setSyncElapsedBeats(beat);
        int goalUnits = track->getSyncGoalUnits();

        // may or may not be a record unit
        Pulse* annotated = barTender->annotate(track, pulse);
        SyncUnit recordUnit = track->getSyncRecordUnit();
        if (recordUnit == SyncUnitNone) {
            // should have stored these when we started all this
            Trace(1, "SyncMaster: Someone forgot to store their units");
            recordUnit = track->getSyncUnitNow();
        }
        if (isRelevant(annotated, recordUnit)) {
            // we don't need both elapsed beats and units since you can
            // derive units from beats, but it's clearer to think in terms
            // of units most of the time
            int elapsed = track->getSyncElapsedUnits() + 1;
            track->setSyncElapsedUnits(elapsed);

            if (goalUnits == 0) {
                // doing an unbounded record
                sendSyncEvent(track, SyncEvent::Extend);
            }
            else if (goalUnits == elapsed) {
                // we've reached the end
                sendSyncEvent(track, SyncEvent::Stop);
            }
            else {
                // interior unit within a known extension
                // don't need to send anything
                pulse = nullptr;
                Trace(2, "SyncMaster: Suppressing pulse %d within goal %d",
                      elapsed, goalUnits);
            }
        }

        // MIDI rounding noise, could do this for all of them
        // but only necessary for MIDI
        SyncSource src = track->getSyncSourceNow();
        if (src == SyncSourceMidi && goalUnits > 0) {
            // midi with a goal, watch for the penultimate beat
            int totalBeats = getGoalBeats(track);
            if (beat == (totalBeats - 1)) {
                midiAnalyzer->lock();
                int startingUnit = track->getUnitLength();
                int endingUnit = midiAnalyzer->getUnitLength();
                if (startingUnit != endingUnit) {
                    Trace(2, "SyncMaster: Adjusting final beat for unit change %d to %d",
                          startingUnit, endingUnit);

                    int idealLength = endingUnit * goalUnits;
                    int currentLength = track->getSyncLength();
                    int unalteredLength = currentLength + endingUnit;
                    if (idealLength == unalteredLength) {
                        // it's a miracle! the unit length fluctuated
                        // but we landed in the right place
                        Trace(2, "SyncMaster: No need to adjust final beat, you should be worried");
                    }
                    else if (currentLength > idealLength) {
                        // you messed something up counting beats
                        Trace(1, "SyncMaster: Ideal length less than where we are now");
                    }
                    else {
                        // this should never be more than a beat, and really a small fraction
                        // unless the user is dicking with the tempo
                        int delta = abs(unalteredLength - idealLength);
                        if (delta > endingUnit)
                          Trace(1, "SyncMaster: Unusually large ending beat adjustment %d",
                                delta);

                        Trace(2, "SyncMaster: Adjusting final beat to end on %d rather than %d",
                              idealLength, unalteredLength);
                        
                        SyncEvent event(SyncEvent::Finalize);
                        event.finalLength = idealLength;
                        track->syncEvent(&event);
                        dealWithSyncEvent(track, &event);

                        // from this point forward we won't send SyncEvents to the track
                        // and it will normally end near the next beat
                        // If the user requests an extension during this 1 beat rounding period
                        // then we'll end up back in requestExtension and start dealing
                        // with the goal units with scheduling rather than sync pulses
                        track->setSyncFinalized(true);
                    }
                }
            }
        }
    }
}

bool SyncMaster::isRelevant(Pulse* p, SyncUnit unit)
{
    bool relevant = false;
    
    if (unit == SyncUnitBeat) {
        // anything is a beat
        relevant = true;
    }
    else if (unit == SyncUnitBar) {
        // loops are also bars
        relevant = (p->unit == SyncUnitBar || p->unit == SyncUnitLoop);
    }
    else {
        // only loops will do
        relevant = (p->unit == SyncUnitLoop);
        // formerly had a fallback to accept Bar units if the
        // host didn't support the concept of a Loop, but they
        // all should now and BarTender will flag it
    }
    return relevant;
}

int SyncMaster::getGoalBeats(LogicalTrack* t)
{
    SyncSource src = t->getSyncSourceNow();
    int unit = t->getSyncUnitNow();
    int units = t->getSyncGoalUnits();
    int beats = 1;
    
    switch (unit) {
        case SyncUnitBeat: {
            beats = units;
        }
            break;
        case SyncUnitNone:
        case SyncUnitBar: {
            int bpb = barTender->getBeatsPerBar(src);
            beats = units * bpb;
        }
            break;
        case SyncUnitLoop: {
            int bpb = barTender->getBeatsPerBar(src);
            int bpl = barTender->getBarsPerLoop(src);
            beats = (units * bpb) * bpl;
        }
            break;
    }
    if (beats == 0) {
        Trace(1, "SyncMaster: Anomolous goal beats calculaation");
        beats = 1;
    }
    return beats;
}

/**
 * True if the sync source for this track has a locked unit.
 * In practice false only for MIDI during the first recording
 * as we let it warm up.
 */
bool SyncMaster::isSourceLocked(LogicalTrack* t)
{
    bool locked = true;
    SyncSource src = t->getSyncSourceNow();
    if (src == SyncSourceMidi)
      locked = midiAnalyzer->isLocked();;
    return locked;
}

/**
 * Send one of the sync events to the track.
 */
void SyncMaster::sendSyncEvent(LogicalTrack* t, SyncEvent::Type type)
{
    SyncEvent event (type);
    
    t->syncEvent(&event);
    
    dealWithSyncEvent(t, &event);
}

/**
 * Called after sending a SyncEvent to a track.
 *
 * !! Hating the "ended" flag, it shouldn't be the event's job to convey this
 * track can just call back to notifyRecordStopped if it stopped.
 *
 * If the track set the error flag, we should abandon the recording.
 */
void SyncMaster::dealWithSyncEvent(class LogicalTrack* lt, SyncEvent* event)
{
    if (event->error) {
        Trace(1, "SyncMaster: SyncEvent returned with errors");
    }
    else if (event->ended) {
        // this is an alternative to the track calling notifyRecordStop
        // though it can do that too
        notifyRecordStopped(lt->getNumber());
    }
}

//////////////////////////////////////////////////////////////////////
//
// Track Notifications
//
// These are expected to be called when a track enters various states.
// This may have side effects if this track is also the TrackSyncMaster
// or TransportMaster.
//
//////////////////////////////////////////////////////////////////////

/**
 * This is called when a track begins recording.
 * If this is the TransportMaster, Synchronizer in the past would do a "full stop"
 * to send a STOP event and stop sending MIDI clocks.
 */
void SyncMaster::notifyRecordStarted(int number)
{
    // continue calling MidiRealizer but this needs to be under the control of the Transport
    if (number == transport->getMaster()) {
        transport->stop();
        // unlike notifyTrackReset, the master connection remains
    }
}

/**
 * This is called when a recording has offically ended.
 * It may have been synced or not.
 *
 * This also makes the track available for mastership.
 */
void SyncMaster::notifyRecordStopped(int number)
{
    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {

        // this stops sending pulses to the track
        if (lt->isSyncRecording()) {
            
            lt->setSyncRecording(false);

            SyncSource src = lt->getSyncSourceNow();
            if (src == SyncSourceMidi) {
                // this one is complicated, verify some things
                if (!midiAnalyzer->isLocked())
                  Trace(1, "SyncMaster: MidiAnalyzer was not locked after recording ended");
                
                int unit = midiAnalyzer->getUnitLength();
                if (unit == 0) {
                    // this is the "first beat recording" fringe case
                    // the end should have been pulsed and remembered
                    Trace(1, "SyncMaster: Expected MIDI to know what was going on by now");
                }
            }

            // verify that the unit length was obeyed
            int trackLength = lt->getSyncLength();
            if (src != SyncSourceTrack) {
                int baseUnit = barTender->getBaseRecordUnitLength(src);
                if (baseUnit == 0) {
                    Trace(1, "SyncMaster: Unable to verify unit length compliance");
                }
                else {
                    int leftover = trackLength % baseUnit;
                    if (leftover != 0) {
                        Trace(1, "SyncMaster: Synchronzied recording leftovers %d", leftover);
                    }
                }
            }
            else {
                // oh we should probably do this too...
                Trace(2, "SyncMaster: Punting on verification of track sync recording");
            }
        }
        
        notifyTrackAvailable(number);
        lt->resetSyncState();
    }
}

/**
 * Called when a track has finished recording and may serve as a sync master.
 *
 * If there is already a sync master, it is not changed, though we should
 * allow a special sync mode, maybe SyncSourceMasterForce or some other
 * parameter that overrides it.
 *
 * Also worth considering if we need an option for tracks to not become the
 * track sync master if they don't want to.
 */
void SyncMaster::notifyTrackAvailable(int number)
{
    // verify the number is in range and can be a leader
    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {
        // anything can become the track sync master
        if (trackSyncMaster == 0)
          trackSyncMaster = number;

        SyncSource src = lt->getSyncSourceNow();
        
        if (src == SyncSourceMaster) {
            // this one wants to be special
            int currentMaster = transport->getMaster();
            if (currentMaster == 0) {

                connectTransport(number);
            }
            else if (currentMaster == number) {
                // this track was already the transport master
                // and we're being told it was re-recorded
                // this can happen if you switch to an empty loop then start
                // a new recording in that loop, 
                // notifyTrackRecord will stop the clocks but it won't take
                // away mastership, it could but I think the intent would be
                // to have this track continue as the master rather than assign
                // another one at random
                connectTransport(number);
            }
            else {
                // this can't be the sync master, it will revert
                // to either SourceLeader or SourceTransport
                // can make that decision later
            }
        }
    }
}

/**
 * Called when a track is reset, if this was one of the sync masters
 * Synchronizer would try to auto-assign another one.
 *
 * This unlocks the follower automatically.
 *
 * If this was the TrackSyncMaster, this is where the old Synchronizer would choose
 * a new one.  Unclear how I want this to work.  We would have to pick one at random
 * which is hard to predict.   Wait until the next new recording which calls
 * notifyTrackAvailable or make the user do it manually.
 *
 * If this was the TransportMaster, old Synchronizer would send a MIDI Stop command.
 *
 * NOTE: This is now being called when the track switches to a loop that is empty.
 * OG Mobius would not stop clocks when that happened.  That seems inconsistent.
 * The loop is "silent" so it is similar to entering Mute mode but we don't have
 * a callback for notifyLoopEmpty or notifyLoopSwitch and then checking for empty.
 * Revisit based on user requests.
 */
void SyncMaster::notifyTrackReset(int number)
{
    if (number == trackSyncMaster) {
        // Synchronizer used to choose a different one automatically
        // It looks like of confusing to see this still show as TrackSyncMaster in the UI
        // so reset it, but don't pick a new one
        trackSyncMaster = 0;
    }

    if (number == transport->getMaster()) {
        // Synchronizer would send MIDI Stop at this point
        // it had a method fullStop that I think both sent the STOP event
        // and stop generating clocks
        transport->stop();

        transport->disconnect();
    }

    // it can no longer be recording
    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr)
      lt->resetSyncState();
}

/**
 * Called when a track has restructured in some way.  Mostly we care about
 * the length of the track loop, but might also be sensitive to cycle counts.
 *
 * This is very similar to notifyTrackAvailable in that when a track moves
 * from Reset to Play after various actions like Switch, Load, or Undo, it could
 * also become a sync master.  Not doing that yet.
 *
 * If the track IS ALREADY the TransportMaster, then the transport must be
 * reconfigured and the tempo may change.
 */
void SyncMaster::notifyTrackRestructure(int number)
{
    if (number == transport->getMaster()) {
        // we don't need to distinguish between restructuring
        // and establishing a connection right now
        connectTransport(number);
    }
}

/**
 * Called when a track Restarts.
 *
 * A Restart means that the track abruptly went to start point through
 * a user action rather than simply playing normally to the end and looping.
 * There are many reasons for this including functions: Start, Realign, StartPoint,
 * UnroundedMultiply, and Trim.  Also LoopSwitch with switchLocation=Start and
 * Unmute with muteMode=Start.
 *
 * When this happens OG Mobius would send MIDI Start if this track was the OutSyncMaster
 * (now the TransportMaster).  The intent here to realign an external MIDI
 * sequencer so that it would play from the start along with the track.
 *
 * This was controlled with another "manual strt" option.  When this was on, it would
 * wait until the user explicitly used the functions MidiStart or MuteMidiStart
 * to schedule when the MIDI Start would be sent.  In those cases the track must call
 * back to notifyMidiStart.
 *
 * todo: Now that the user can interact with the transport using the UI, there are other
 * ways to request a MIDI start.  These are not the same as using the old MidiStart function
 * and do not currrently have any synchronization options.  I think I'd like to keep it that
 * way, if the user presses the Start button, it should just start.  If this suspends waiting
 * for something in the master track, then we have yet another form of event scheduling
 * and "synchronize mode" that would have to be visualized in the UI.  That's what the
 * MidiStart function is for, though a better name for that might be SyncMidiStart or
 * RequestMidiStart.
 *
 * While transportManualStart is technically a Transport parameter, we test it out here
 * because SyncMaster has the context necessary to know whether this is an auto-start
 * or an explicit start.
 */
void SyncMaster::notifyTrackRestart(int number)
{
    if (number == transport->getMaster()) {
        if (!manualStart) 
          transport->start();
    }
}

/**
 * Callback for the MidiStart and MuteMidiStart functions.  The user explicitly
 * asked for a Start so we don't test ManualStart.
 *
 * OG Mobius had some thinking around "checkNear" which tried to determine if the
 * external MIDI loop was already near the start point and if so avoided sending
 * a redundant MIDI Start which could cause "flamming" of drum notes that had already
 * been recently played.  This might sound better to the user, but it would put the
 * external pattern slightly out of alignment with where the master loop is.
 * I decided not to carry that forward but it could be useful.  If you do decide
 * to bring that back it should apply to notifyTrackRestart as well.
 */
void SyncMaster::notifyMidiStart(int number)
{
    // does this have to be the TransportMaster or can it be sent from anywhere?
    (void)number;
    transport->start();
}

/**
 * Called when the track has entered a state of Pause.
 * This can happen with the Pause or GlobalPause functions, or the Stop
 * function which both pauses and rewinds.
 *
 * It also happens indirectly when a project is loaded and puts all tracks
 * in Pause mode.
 *
 * OG Synchronzier had MuteSyncMode that would control whether to stop clocks
 * whenever the loop became silent.  In the EDP this was only for Mute mode
 * but I extended it to apply to Pause as well.  Since SyncMaster is handling
 * ManualStart it also needs transportMuteStop to control what happens when
 * in Mute/Pause modes.  For now, assuming we stop.
 */
void SyncMaster::notifyTrackPause(int number)
{
    if (number == transport->getMaster()) {
        
        // todo: transportMuteStop parmater to disable this
        transport->stop();
    }
}

/**
 * Called when the track exists Pause.
 *
 * OG Synchronizer didn't do anything special here, but this is the place
 * where it should be trying to send SongPositionPointer.
 *
 * The complication is that MIDI Continue requires a song position pointer,
 * and these are coarser grained than an audio stream frame location
 * the MIDI Continue would need to be delayed until the Transport actually
 * reaches that song position.
 */
void SyncMaster::notifyTrackResume(int number)
{
    if (number == transport->getMaster()) {
        // !! probably wrong
        transport->start();
    }
}

/**
 * Called when a track enters Mute mode.
 *
 * Old Synchronizer had MuteSyncMode options to send a MIDI Stop event when this happened,
 * and then other options about what happened when the track unmuted.
 * Those should be moved to Transport parameters.  As it stands now, unmute
 * options are internal to Mobius and it will call back to Start or Resume.
 */
void SyncMaster::notifyTrackMute(int number)
{
    // punt for now
    (void)number;
}

/**
 * Called when a track jumps to a new location rather than advancing normally.
 * This could be used to send MIDI song position pointers which is hard.
 *
 * Making this different than notifyTrackStart which is overloaded with other options.
 */
void SyncMaster::notifyTrackMove(int number)
{
    (void)number;
}

/**
 * Called when a track changes playback rate.
 *
 * In theory this could adjust the tempo of the Transport and MIDI clocks.
 * Don't remember what old Synchronizer did, it probably tried to change the tempo
 * poorly.
 */
void SyncMaster::notifyTrackSpeed(int number)
{
    (void)number;
}

/**
 * This is called when OG Mobius evaluates the MidiStop function event.
 *
 * Comments say this was "usually scheduled for the start point" but I don't
 * see that happening.  It would either happen immediately or stacked on a loop
 * switch, and probably after RecordStopEvent latency.
 *
 * A better name for this would be SyncMidiStop to distinguish it from
 * TransportStop which is always immediate.
 *
 * !! The old functions with "Midi" in their names need to be revisited.
 */
void SyncMaster::notifyMidiStop(int number)
{
    (void)number;
}

//////////////////////////////////////////////////////////////////////
//
// Old Synchronizer Code
//
// This is here for reference only, showing what it used to do and we
// need to replicate in some way.
//
//////////////////////////////////////////////////////////////////////

/**
 * Copied from the old Synchroonzer for reference
 *
 * It is assumed we've already checked for the track being the TransportMaster
 * and something happened that requires a MIDI start.
 *
 * The checkManual flag says to look at an old config option to suppress
 * automatic starts.
 *
 * The checkNear flag is stubbed and used the SyncTracker which no longer exists.
 * I think the intent was to suppress starts when we know that we're already pretty k
 * close to the start point, I guess to prevent flamming if you're driving a drum machine.
 */
#if 0
void SyncMaster::sendStart(bool checkManual, bool checkNear)
{
	bool doStart = true;

    // probably still want this, but get it from the Session
	if (checkManual) {
        //Setup* setup = mMobius->getActiveSetup();
        //doStart = !(setup->isManualStart());
	}

	if (doStart) {
		// To avoid a flam, detect if we're already at the external
		// start point so we don't need to send a START.
        // !! We could be a long way from the pulse, should we be
        // checking frame advance as well?
        
        bool nearStart = false;
        if (checkNear) {
            /*
            int pulse = mOutTracker->getPulse();
            if (pulse == 0 || pulse == mOutTracker->getLoopPulses())
              nearStart = true;
            */
        }

        if (nearStart && isMidiOutStarted()) {
			// The unit tests want to verify that we at least tried
			// to send a start event.  If we suppressed one because we're
			// already there, still increment the start count.
            Trace(2, "SyncMaster: Suppressing MIDI Start since we're near\n");
			midiRealizer->incStarts();
        }
        else {
            Trace(2, "SyncMaster: Sending MIDI Start\n");
            // mTransport->start(l);
            midiRealizer->start();
		}
	}
}

/**
 * Called when a track pauses.
 * If this is the TransportMaster we have sent MIDI Stop
 * !! this needs to be moved inside Transport
 */
void SyncMaster::notifyTrackStop(int number)
{
    if (transport->getMaster() == number) {
        muteMidiStop();
    }
}

/**
 * After entering Mute or Pause modes, decide whether to send
 * MIDI transport commands and stop clocks.  This is controlled
 * by an obscure option MuteSyncMode.  This is for dumb devices
 * that don't understand STOP/START/CONTINUE messages.
 *
 * todo: decide if we still need this
 */
void SyncMaster::muteMidiStop()
{

    //Setup* setup = mMobius->getActiveSetup();
    //MuteSyncMode mode = setup->getMuteSyncMode();
    MuteSyncMode mode = MUTE_SYNC_TRANSPORT;

    bool doTransport = (mode == MUTE_SYNC_TRANSPORT ||
                        mode == MUTE_SYNC_TRANSPORT_CLOCKS);
    
    bool doClocks = (mode == MUTE_SYNC_CLOCKS ||
                     mode == MUTE_SYNC_TRANSPORT_CLOCKS);

    // mTransport->stop(l, transport, clocks);
    // what "transport" means here is sending MIDI start/stop messages
    midiRealizer->stopSelective(doTransport, doClocks);
}

/**
 * Called when a track resumes from Pause
 */
void SyncMaster::notifyTrackResume(int number)
{
    // !! this needs to be moved inside Transport
    if (transport->getMaster() == number) {
        //Setup* setup = mMobius->getActiveSetup();
        //MuteSyncMode mode = setup->getMuteSyncMode();
        MuteSyncMode mode = MUTE_SYNC_TRANSPORT;
        
        if (mode == MUTE_SYNC_TRANSPORT ||
            mode == MUTE_SYNC_TRANSPORT_CLOCKS) {
            // we sent MS_STOP, now send MS_CONTINUE
            //mTransport->midiContinue(l);
            midiRealizer->midiContinue();
        }
        else  {
            // we just stopped sending clocks, resume them
            // mTransport->startClocks(l);
            midiRealizer->startClocks();
        }
    }
}

/**
 * Called by Loop when it enters Mute mode.
 * 
 * When MuteMode=Start the EDP would stop clocks then restart them
 * when we restart comming out of mute.  Feels like another random
 * EDPism we don't necessarily want, should provide an option to keep
 * clocks going and restart later.
 */
void SyncMaster::notifyTrackMute(int number)
{
	if (transport->getMaster() == number) {
		//Preset* p = l->getPreset();
        //MuteMode mode = p->getMuteMode();
        ParameterMuteMode mode = MUTE_START;
        
		if (mode == MUTE_START) 
          muteMidiStop();
	}
}
#endif

//////////////////////////////////////////////////////////////////////
//
// Masters
//
//////////////////////////////////////////////////////////////////////

// !! Need to think more about the concepts of Connect and Disconnect
// for the TransportMaster

/**
 * There can be one TrackSyncMaster.
 *
 * This becomes the default leader track when using SyncSourceLeader
 * and the follower didn't specify a specific leader.
 * 
 * It may be changed at any time.
 */
void SyncMaster::setTrackSyncMaster(int leaderId)
{
    int oldMaster = trackSyncMaster;
    trackSyncMaster = leaderId;

    // changing this may result in reordering of tracks
    // during an advance
    if (oldMaster != trackSyncMaster) {
        timeSlicer->syncFollowerChanges();
    }
}

int SyncMaster::getTrackSyncMaster()
{
    return trackSyncMaster;
}

/**
 * Action handler for FuncSyncMasterTrack
 * Formerly implemented as a Mobius core function.
 * This took no arguments and made the active track the master.
 *
 * Now this makes the focused track the master which may include MIDI tracks.
 * To allow more control, the action may have an argument with a track number.
 * todo: This needs to be expanded to accept any form of trackk identifier.
 */
void SyncMaster::setTrackSyncMaster(UIAction* a)
{
    int number = a->value;
    if (number == 0) {
        // todo: not liking how track focus is passed around and where it lives
        number = container->getFocusedTrackIndex() + 1;
    }

    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt == nullptr)
      Trace(1, "SyncMaster: Invalid track id in SyncMasterTrack action");
    else
      setTrackSyncMaster(number);
}

/**
 * There can only be one Transport Master.
 * Changing this may change the tempo of geneerated MIDI clocks if the transport
 * has MIDI enabled.
 */
void SyncMaster::setTransportMaster(int id)
{
    if (transport->getMaster() != id) {

        if (id > 0)
          connectTransport(id);
        else {
            // unusual, they are asking to not have a sync master
            // what else should happen here?  Stop it?
            transport->disconnect();
        }
    }
}

/**
 * Connection between a track and the transport is done
 * by giving Transport the TrackProperties.
 */
void SyncMaster::connectTransport(int id)
{
    TrackManager* tm = kernel->getTrackManager();
    TrackProperties props;
    tm->getTrackProperties(id, props);
    transport->connect(props);
}

int SyncMaster::getTransportMaster()
{
    return transport->getMaster();
}

/**
 * Action handler for FuncSyncMasterMidi
 * Formerly implemented as a Mobius core function.
 * This took no arguments and made the active track the "MIDI Master".
 *
 * This is now the equivalent of setting the TransportMaster.
 * The name "SyncMasterMidi" is kept for backward compatibility but it should
 * be made an alias of TransportMaster.
 *
 * Like SyncMasterTrack this now makes the focused track the master which may
 * include MIDI tracks.
 */
void SyncMaster::setTransportMaster(UIAction* a)
{
    int number = a->value;
    if (number == 0) {
        // todo: not liking how track focus is passed around and where it lives
        number = container->getFocusedTrackIndex() + 1;
    }

    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt == nullptr)
      Trace(1, "SyncMaster: Invalid track id in TransportMaster action");
    else {
        setTransportMaster(number);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

int SyncMaster::getBlockCount()
{
    return blockCount;
}

/**
 * This must be called very early in the kernel block processing phase.
 * It initializes the subcomponents for the call to processAudioStream() which
 * happens after various things in the kernel, in particular after
 * any action handling which may assign sync sources to tracks.
 *
 * It is important that sync pulses be analyzed BEFORE actions are processed
 * so that the initiation of synchronized recordings has the updated
 * sync state.
 */
void SyncMaster::beginAudioBlock(MobiusAudioStream* stream)
{
    blockCount++;
    
    // monitor changes to the sample rate once the audio device is pumping
    // and adjust internal calculations
    // this should come through MobiusAudioStream, that's where it lives
    int newSampleRate = stream->getSampleRate();
    if (newSampleRate != sampleRate)
      refreshSampleRate(newSampleRate);

    // once we start receiving audio blocks, it is okay to start converting
    // MIDI events into MidiSyncMessages, if you allow event queueing before
    // blocks come in, the queue can overflow
    // !! this is old and needs to go away, no longer used by MidiAnalyzer
    // and MidiRealizer needs to stop
    enableEventQueue();
    
    // tickle the analyzers

    // Detect whether MIDI clocks have stopped comming in
    //
    // Supervisor formerly called this on the maintenance thread
    // interval, since we don't get a performMaintenance ping down here
    // we can check it on every block, the granularity doesn't really matter
    // since it is based off millisecond time advance
    // !! why can't this just be done in analyze() now?
    midiAnalyzer->checkClocks();

    int frames = stream->getInterruptFrames();
    hostAnalyzer->analyze(frames);
    midiAnalyzer->analyze(frames);
    // Transport should be controlling this but until it does it is important
    // to get the event queue consumed, Transport can just ask for the Result
    // when it advances
    midiRealizer->advance(frames);
    transport->advance(frames);
    barTender->advance(frames);
    pulsator->advance(frames);

    // see commentary about why this is complicated
    // no longer necessary, Transport does it's own drift checking
    //transport->checkDrift(frames);

    // temporary diagnostics
    checkDrifts();
}

void SyncMaster::refreshSampleRate(int rate)
{
    sampleRate = rate;
    
    hostAnalyzer->setSampleRate(rate);
    transport->setSampleRate(rate);
    midiRealizer->setSampleRate(rate);
    midiAnalyzer->setSampleRate(rate);
}

/**
 * Here after actions have been performed, events have been scheduled
 * and we're ready to advance the tracks.
 *
 * This process is controlled by TimeSlicer.
 */
void SyncMaster::processAudioStream(MobiusAudioStream* stream)
{
    timeSlicer->processAudioStream(stream);
}

/**
 * Called by Transport whenever it starts as the result of an action.
 * Since this happens after Pulsator was advanced in beginAudioBlock, have
 * to ask it to look again.
 */
void SyncMaster::notifyTransportStarted()
{
    pulsator->notifyTransportStarted();
}

/**
 * The event queue should only be enabled once audio blocks
 * start comming in.  If blocks stop then the queue can overflow
 * if there is MIDI being actively received or sent.
 *
 * Block stoppage can't be monitored here, it would need to be done
 * by a higher power, probablky the maintenance thread.
 */
void SyncMaster::enableEventQueue()
{
    midiRealizer->enableEvents();
}

void SyncMaster::disableEventQueue()
{
    midiRealizer->disableEvents();
}

//////////////////////////////////////////////////////////////////////
//
// Shell Requests
//
//////////////////////////////////////////////////////////////////////

/**
 * The only action receiver right now is Transport.
 * There isn't anything about host/midi sync that is under the user's
 * control.
 */
bool SyncMaster::doAction(UIAction* a)
{
    bool handled = true;
    
    switch (a->symbol->id) {

        case FuncSyncMasterTrack:
            setTrackSyncMaster(a);
            break;

        case FuncSyncMasterTransport:
            setTransportMaster(a);
            break;
            
        default: {
            handled = transport->doAction(a);
            if (!handled)
              handled = barTender->doAction(a);
        }
            break;
    }
    
    return handled;
}

/**
 * We don't seem to have had parameters for trackSyncMaster and outSyncMaster,
 * those were implemented as script variables.  If they were parameters it would
 * make it more usable for host parameter bindings.
 */
bool SyncMaster::doQuery(Query* q)
{
    bool handled = false;

    // todo: the masters, host and midi settings

    handled = transport->doQuery(q);
    if (!handled)
      handled = barTender->doQuery(q);
    
    return handled;
}

/**
 * Add state for each sync source.
 * Also handling sync state for each Track since we're in a good position to do that
 * and don't need to bother the BaseTracks with knowing the details.
 */
void SyncMaster::refreshState(SystemState* sysstate)
{
    SyncState* state = &(sysstate->syncState);
    
    state->transportMaster = transport->getMaster();
    state->trackSyncMaster = trackSyncMaster;

    // the MidiSyncElement wants to display normalized beat/bar/loop numbers
    // and this is not track specific
    // !! need to seriously rethink kthe utility of track-specific BPB and BPL
    // overrides, why can't this just be global?  it only really matters
    // for the initial recording, then it's just for display
    
    // Analyzer fills everything except normalized beats
    midiAnalyzer->refreshState(state);
    state->midiBeat = barTender->getBeat(SyncSourceMidi);
    state->midiBar = barTender->getBar(SyncSourceMidi);
    state->midiLoop = barTender->getLoop(SyncSourceMidi);
    state->midiBeatsPerBar = barTender->getBeatsPerBar(SyncSourceMidi);
    state->midiBarsPerLoop = barTender->getBarsPerLoop(SyncSourceMidi);
    
    // the host doesn't have a UI element since you're usually just watching the
    // host UI, but if you have overrides it should
    // same issues about global vs. track-specific time signatures as MIDI sync
    state->hostStarted = hostAnalyzer->isRunning();
    state->hostTempo = hostAnalyzer->getTempo();

    // transport maintains all this inside itself because the time signaturek
    // adapts to the connected loop rather than being always controlled from
    // Session parameters
    transport->refreshState(state);

    int totalTracks = trackManager->getTrackCount();
    int maxStates = sysstate->tracks.size();

    if (maxStates < totalTracks) {
        Trace(1, "SyncMaster: Not enough TrackStates for sync state");
        totalTracks = maxStates;
    }
    
    for (int i = 0 ; i < totalTracks ; i++) {
        TrackState* tstate = sysstate->tracks[i];
        int trackNumber = i+1;

        // we have so far been the one to put sync state in SystemState
        // but since this is all on the LogicalTrack now TM could do it

        LogicalTrack* lt = trackManager->getLogicalTrack(trackNumber);
        if (lt != nullptr) {
            tstate->syncSource = lt->getSyncSourceNow();
            tstate->syncUnit = lt->getSyncUnitNow();

            // old convention was to suppress beat/bar display
            // if the source was not in a started state
            bool running = true;
            if (tstate->syncSource == SyncSourceMidi)
              running = midiAnalyzer->isRunning();
            else if (tstate->syncSource == SyncSourceHost)
              running = hostAnalyzer->isRunning();

            // the convention has been that if beat or bar are
            // zero they are undefined and not shown, TempoElement assumes this
            if (running) {
                tstate->syncBeat = barTender->getBeat(lt) + 1;
                tstate->syncBar = barTender->getBar(lt) + 1;
            }
            else {
                tstate->syncBeat = 0;
                tstate->syncBar = 0;
            }
        }
    }
}

void SyncMaster::refreshPriorityState(PriorityState* pstate)
{
    transport->refreshPriorityState(pstate);

    pstate->midiBeat = barTender->getBeat(SyncSourceMidi);
    pstate->midiBar = barTender->getBar(SyncSourceMidi);
    pstate->midiLoop = barTender->getLoop(SyncSourceMidi);
}

//////////////////////////////////////////////////////////////////////
//
// Internal Component Services
//
//////////////////////////////////////////////////////////////////////

/**
 * MidiRealizer does this for MIDI device issues.
 * This needs to end up in Supervisor::addAlert and handled in the
 * UI thread.
 */
void SyncMaster::sendAlert(juce::String msg)
{
    kernel->sendAlert(msg);
}

/**
 * Must be called during track advance by anything that can lead.  Will
 * be ignored unless something is following it.
 */
void SyncMaster::addLeaderPulse(int leader, SyncUnit unit, int frameOffset)
{
    pulsator->addLeaderPulse(leader, unit, frameOffset);
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
int SyncMaster::getActiveFollowers(SyncSource src, int unitLength)
{
    int followers = 0;
    
    for (int i = 0 ; i < trackManager->getTrackCount() ; i++) {
        LogicalTrack* lt = trackManager->getLogicalTrack(i+1);
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

//////////////////////////////////////////////////////////////////////
//
// Old core Variable Support
//
// These are old and should only be used for some core script Variables.
// Weed these out in time.
// I put "var" in front of them to make it clear what they're intended for
// the rest of the system shouldn't be using these.
//
//////////////////////////////////////////////////////////////////////

bool SyncMaster::varIsMidiOutSending()
{
    return midiRealizer->isSending();
}
bool SyncMaster::varIsMidiOutStarted()
{
    return midiRealizer->isStarted();
}

int SyncMaster::varGetMidiInRawBeat()
{
    return midiAnalyzer->getElapsedBeats();
}
bool SyncMaster::varIsMidiInReceiving()
{
    return midiAnalyzer->isReceiving();
}
bool SyncMaster::varIsMidiInStarted()
{
    return midiAnalyzer->isRunning();
}

//
// Interfaces that take just a SyncSource are obsolete and
// only used by old core/Variable and core/Synchronizer code
// These will be phased out
//

int SyncMaster::varGetBeat(SyncSource src)
{
    (void)src;
    return barTender->getBeat(0);
}

int SyncMaster::varGetBar(SyncSource src)
{
    (void)src;
    return barTender->getBar(0);
}

int SyncMaster::varGetBeatsPerBar(SyncSource src)
{
    (void)src;
    return barTender->getBeatsPerBar(0);
}

float SyncMaster::varGetTempo(SyncSource src)
{
    float tempo = 0.0f;
    switch (src) {
        
        case SyncSourceHost: {
            tempo = (float)hostAnalyzer->getTempo();
        }
            break;

        case SyncSourceMidi: {
            // Pulsator also tracks this but we can get it directly from the Analyzer
            tempo = midiAnalyzer->getTempo();
        }
            break;

        case SyncSourceMaster:
        case SyncSourceTransport: {
            // these are now the same
            tempo = transport->getTempo();
        }
            break;

        default: break;
    }
    return tempo;
}

//////////////////////////////////////////////////////////////////////
//
// Drift Testing
//
//////////////////////////////////////////////////////////////////////

/**
 * At the end of each block, look at the various sync sources that
 * can have drift and if they have reached a loop or bar point in this
 * block, trace the current drift.
 *
 * It doesn't really matter when we trace drift, it just needs to come
 * out at interesting moments and not too fast.o
 */
void SyncMaster::checkDrifts()
{
    SyncAnalyzerResult* res = hostAnalyzer->getResult();
    if (res->beatDetected) {
        // every 4 beats is good enough for now
        int beat = hostAnalyzer->getElapsedBeats();
        if ((beat % 4) == 0) {
            int drift = hostAnalyzer->getDrift();
            Trace(2, "SyncMaster: Host drift %d", drift);
        }
    }
}


//////////////////////////////////////////////////////////////////////
//
// Auto Record
//
//////////////////////////////////////////////////////////////////////

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
