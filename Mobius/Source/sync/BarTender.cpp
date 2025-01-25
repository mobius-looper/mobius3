/**
 * Gather the incredible mess into one place and sort it out.
 *
 * There are two fundaantal things BarTender does:
 *
 *    1) Knows what each track considers to be the "beats per bar" and massages
 *       raw Pulses from the sync sources into pulses that have bar and loop flags
 *       set on them correctly
 *
 *    2) Knows what the normalized beat and bar numbers are for each track
 *       and provides them through SystemState for display purposes
 *
 */

#include <JuceHeader.h>

#include "../util/Trace.h"

#include "../model/SessionConstants.h"
#include "../model/Session.h"

#include "../mobius/track/TrackManager.h"
#include "../mobius/track/LogicalTrack.h"
#include "../mobius/track/TrackProperties.h"

#include "Pulse.h"
#include "Transport.h"
#include "HostAnalyzer.h"
#include "MidiAnalyzer.h"
#include "SyncAnalyzerResult.h"
#include "SyncMaster.h"

#include "BarTender.h"

BarTender::BarTender(SyncMaster* sm, TrackManager* tm)
{
    syncMaster = sm;
    trackManager = tm;
}

BarTender::~BarTender()
{
}

/**
 * The first was intended to be the BPB for the Transport, but that can
 * go out the window if the Transport locks onto a master track.  That new
 * value isn't in the Session so if you edit the Session that will get pushed
 * back to the Transport.
 * Needs thought...
 *
 * Problem 2: Pulsator assumes followers are abstract things that aren't
 * necessarily tracks but BarTender does assume they are tracks and follower
 * numbers can be used as indexes in to the Session.  For all purposes, there
 * is no difference between a follower and a track, but may need more here.
 *
 */
void BarTender::loadSession(Session* s)
{
    // !! todo: all these need to be actionable for scripts
    
    hostBeatsPerBar = s->getInt(SessionHostBeatsPerBar);
    hostBarsPerLoop = s->getInt(SessionHostBarsPerLoop);
    hostOverride = s->getBool(SessionHostOverride);

    midiBeatsPerBar = s->getInt(SessionMidiBeatsPerBar);
    midiBarsPerLoop = s->getInt(SessionMidiBarsPerLoop);
}

/**
 * During the advance phase we can detect whether the Host
 * made a native time signature change.  If the BPB for the host
 * is not overridden, this could adjust bar counters for tracks that
 * follow the host.
 */
void BarTender::advance(int frames)
{
    (void)frames;
    
    // reflect changes in the Host time signature if they were detected
    HostAnalyzer* anal = syncMaster->getHostAnalyzer();
    SyncAnalyzerResult* result = anal->getResult();
    if (result->timeSignatureChanged) {

        // what exactly would we do with this?
        // if we compute beat/bar numbers when accessed then
        // we don't really need to adjust anything, if they are
        // saved in each BarTender::Track then we would adjust them now
    }

    // the Transport can also manage a time signature, if you need to
    // do it for Host, you need it there too
    
}

//////////////////////////////////////////////////////////////////////
//
// Pulse Annotation
//
//////////////////////////////////////////////////////////////////////

Pulse* BarTender::annotate(LogicalTrack* lt, Pulse* beatPulse)
{
    Pulse* result = beatPulse;
    Transport* transport = syncMaster->getTransport();
    bool onBar = false;
    bool onLoop = false;
    
    switch (lt->getSyncSourceNow()) {
        case SyncSourceNone:
            // shouldn't be here
            break;

        case SyncSourceMidi: {
            // it would actually be nice to have the Analyzer return
            // the elapsed beat count which would then be saved in the Pulse
            // so we don't have to go back there to get it
            MidiAnalyzer* anal = syncMaster->getMidiAnalyzer();
            int raw = anal->getElapsedBeats();
            int bpb = getMidiBeatsPerBar();
            onBar = ((raw % bpb) == 0);
            if (onBar) {
                int bpl = getMidiBarsPerLoop();
                int beatsPerLoop = bpb * bpl;
                onLoop = ((raw & beatsPerLoop) == 0);
            }
        }
            break;

        case SyncSourceTransport:
        case SyncSourceMaster: {
            // Transport did the work for us
            SyncAnalyzerResult* res =  transport->getResult();
            onBar = res->barDetected;
            onLoop = res->loopDetected;
        }
            break;

        case SyncSourceHost: {
            // armegeddon
            detectHostBar(onBar, onLoop);
        }
            break;

        case SyncSourceTrack: {
            // Leader pulses were added by the leader track and should
            // have already have the right unit in them, Bar corresponding
            // to cycle and Loop corresponding to the loop start
            // there isn't anything further we need to provide
        }
            break;
    }

    if (onBar || onLoop) {
        // copy the original pulse and change it's unit
        annotated = *beatPulse;
        if (onLoop)
          annotated.unit = SyncUnitLoop;
        else
          annotated.unit = SyncUnitBar;
        result = &annotated;
    }

    return result;
}

/**
 * Finally folks, the reason I brought you all here...
 *
 * Deciding whether the host has reached a "bar" has numerous complications,
 * especially for "looping" hosts like FL Studio.  Here the native beat
 * number can jump between two points often back to zero but really any two
 * beats.
 *
 * alexs1 has some specific desires around this, and there was some forum discussion
 * on various options.  Basically you can take the host beat number and do the
 * usual modulo, OR you can simply count beats from the start point.
 *
 * For initail testing, we'll just do the usual modulo
 */
void BarTender::detectHostBar(bool &onBar, bool& onLoop)
{
    int bpb = getHostBeatsPerBar();
    HostAnalyzer* anal = syncMaster->getHostAnalyzer();

    // here we have the option of basing this on the elapsed beat count
    // or the native beat number, same for getBeat below
    int raw = anal->getNativeBeat();

    onBar = ((raw % bpb) == 0);

    if (onBar) {
        int bpl = getHostBarsPerLoop();
        int beatsPerLoop = bpb * bpl;
        onLoop = ((raw & beatsPerLoop) == 0);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Time Signature Determination
//
//////////////////////////////////////////////////////////////////////

int BarTender::getHostBeatsPerBar()
{
    // first calculate the default in case the host doesn't tell us
    int bpb = hostBeatsPerBar;
    if (bpb < 1) {
        // I guess fall back to the transport
        bpb = syncMaster->getTransport()->getBeatsPerBar();
    }

    if (!hostOverride) {
        // not using the default, ask the host
        HostAnalyzer* anal = syncMaster->getHostAnalyzer();
        if (anal->hasNativeTimeSignature())
          bpb = anal->getNativeBeatsPerBar();
        // else use the default
    }

    // final sanity check
    if (bpb < 1) bpb = 4;
    return bpb;
}

int BarTender::getHostBarsPerLoop()
{
    // hosts don't have a way to say this though there are some
    // obscure ones related to looping modes that might be useful
    int bpl = hostBarsPerLoop;
    if (bpl < 1) bpl = 1;
    return bpl;
}

int BarTender::getMidiBeatsPerBar()
{
    int bpb = midiBeatsPerBar;
    if (bpb < 1) bpb = 4;
    return bpb;
}

int BarTender::getMidiBarsPerLoop()
{
    int bpl = midiBarsPerLoop;
    if (bpl < 1) bpl = 1;
    return bpl;
}

//////////////////////////////////////////////////////////////////////
//
// Normalized Beats
//
//////////////////////////////////////////////////////////////////////

int BarTender::getBeat(int trackNumber)
{
    return getBeat(trackManager->getLogicalTrack(trackNumber));
}

/**
 * Should be maintaining these on each advance, watching for sync pulses
 * for each track and advancing our own counters in Track.  But until then
 * just math the damn things every time.
 */
int BarTender::getBeat(LogicalTrack* lt)
{
    int beat = 0;
    
    Transport* transport = syncMaster->getTransport();
    
    if (lt != nullptr) {
        switch (lt->getSyncSourceNow()) {
            case SyncSourceNone:
                // technically should return zero?
                break;

            case SyncSourceMidi: {
                MidiAnalyzer* anal = syncMaster->getMidiAnalyzer();
                int raw = anal->getElapsedBeats();
                if (raw > 0) {
                    int bpb = getMidiBeatsPerBar();
                    beat = (raw % bpb);
                }
            }
                break;

            case SyncSourceTransport:
            case SyncSourceMaster: {
                // this maintains it it's own self
                beat = transport->getBeat();
            }
                break;
                
            case SyncSourceHost: {
                HostAnalyzer* anal = syncMaster->getHostAnalyzer();

                // see detectHostBar for some words about the difference
                // between elapsed beat and nativeBeat here
                // may need more options
                int raw = anal->getElapsedBeats();
                int bpb = getHostBeatsPerBar();
                beat = (raw % bpb);
            }
                break;

            case SyncSourceTrack: {
                // unclear what this means, it could be the subcycle number from
                // the leader track, 
                // but really we shouldn't be trying to display beat/bar counts
                // in the UI if this isn't following somethign with well defined beats
            }
                break;
        }
    }

    return beat;
}

int BarTender::getBar(int trackNumber)
{
    return getBar(trackManager->getLogicalTrack(trackNumber));
}

int BarTender::getBar(LogicalTrack* lt)
{
    int bar = 0;
    
    Transport* transport = syncMaster->getTransport();
    
    if (lt != nullptr) {
        switch (lt->getSyncSourceNow()) {
            case SyncSourceNone:
                // technically should return zero?
                break;

            case SyncSourceMidi: {
                MidiAnalyzer* anal = syncMaster->getMidiAnalyzer();
                int raw = anal->getElapsedBeats();
                if (raw > 0) {
                    int bpb = getMidiBeatsPerBar();
                    bar = (raw / bpb);
                }
            }
                break;

            case SyncSourceTransport:
            case SyncSourceMaster:
                // this maintains it it's own self
                bar = transport->getBar();
                break;
                
            case SyncSourceHost: {
                HostAnalyzer* anal = syncMaster->getHostAnalyzer();
                int raw = anal->getElapsedBeats();
                int bpb = getHostBeatsPerBar();
                bar = (raw / bpb);
            }
                break;

            case SyncSourceTrack: {
                // unclear what this means, it could be the subcycle number from
                // the leader track, 
                // but really we shouldn't be trying to display beat/bar counts
                // in the UI if this isn't following somethign with well defined beats
            }
                break;
        }
    }

    return bar;
}

int BarTender::getLoop(int trackNumber)
{
    return getLoop(trackManager->getLogicalTrack(trackNumber));
}

int BarTender::getLoop(LogicalTrack* lt)
{
    int loop = 0;
    
    Transport* transport = syncMaster->getTransport();
    
    if (lt != nullptr) {
        switch (lt->getSyncSourceNow()) {
            case SyncSourceNone:
                break;

            case SyncSourceMidi: {
                MidiAnalyzer* anal = syncMaster->getMidiAnalyzer();
                int raw = anal->getElapsedBeats();
                if (raw > 0) {
                    int bpb = getMidiBeatsPerBar();
                    int bpl = getMidiBarsPerLoop();
                    int beatsPerLoop = bpb * bpl;
                    loop = (raw / beatsPerLoop);
                }
            }
                break;

            case SyncSourceTransport:
            case SyncSourceMaster:
                // this maintains it it's own self
                loop = transport->getLoop();
                break;
                
            case SyncSourceHost: {
                HostAnalyzer* anal = syncMaster->getHostAnalyzer();
                // todo: this has the host bar number vs. elapsed origin issue?
                int raw = anal->getElapsedBeats();
                int bpb = getHostBeatsPerBar();
                int bpl = getHostBarsPerLoop();
                int beatsPerLoop = bpb * bpl;
                loop = (raw / beatsPerLoop);
            }
                break;

            case SyncSourceTrack: {
                // we don't remember the number of times these iterate
            }
                break;
        }
    }

    return loop;
}

/**
 * Punting on track overrides for awhile.
 */
int BarTender::getBeatsPerBar(int trackNumber)
{
    int bpb = 4;
    
    LogicalTrack* lt = trackManager->getLogicalTrack(trackNumber);
    if (lt != nullptr) {
        switch (lt->getSyncSourceNow()) {
            case SyncSourceNone:
                // technically should return zero?
                break;

            case SyncSourceMidi:
                bpb = getMidiBeatsPerBar();
                break;
                
            case SyncSourceTransport:
            case SyncSourceMaster:
                bpb = syncMaster->getTransport()->getBeatsPerBar();
                break;
                
            case SyncSourceHost: {
                bpb = getHostBeatsPerBar();
            }
                break;

            case SyncSourceTrack: {
                // unclear what this means, it could be the subcycle count
                // of the leader track, but that's pretty random, or it could
                // fall back to the Transport
                // but really we shouldn't be trying to display beat/bar counts
                // in the UI if this isn't following somethign with well defined beats
            }
                break;
        }
    }
    
    // since this is commonly used for division, always be sure
    // it has life
    if (bpb <= 0)
      bpb = 4;

    return bpb;
}

/**
 * Mostly for transport, but can also apply the notion of a loop
 * or "pattern length" to MIDI and Host.
 * 
 * For leaders, I guess return the cycle count, though
 * getBeatsPerBar with a sync leader doesn't normally return
 * the leader's subcycle count.
 */
int BarTender::getBarsPerLoop(int trackNumber)
{
    int bpl = 1;
    
    LogicalTrack* lt = trackManager->getLogicalTrack(trackNumber);
    if (lt != nullptr) {
        switch (lt->getSyncSourceNow()) {
            case SyncSourceNone:
                break;

            case SyncSourceMidi:
                bpl = getMidiBarsPerLoop();
                break;
                
            case SyncSourceTransport:
            case SyncSourceMaster:
                bpl = syncMaster->getTransport()->getBarsPerLoop();
                break;
                
            case SyncSourceHost: {
                bpl = getHostBarsPerLoop();
            }
                break;

            case SyncSourceTrack: {
                TrackProperties props;
                getLeaderProperties(trackNumber, props);
                if (!props.invalid && props.cycles > 0)
                  bpl = props.cycles;
            }
                break;
        }
    }

    if (bpl < 1) bpl = 1;
    return bpl;
}

/**
 * Not sure why this is here, but don't need it any more
 */
SyncSource BarTender::getSyncSource(int trackNumber)
{
    Trace(1, "BarTender::getSyncSource Who calls this?");
    SyncSource source = SyncSourceNone;
    LogicalTrack* lt = trackManager->getLogicalTrack(trackNumber);
    if (lt != nullptr)
      source = lt->getSyncSourceNow();
    return source;
}

void BarTender::getLeaderProperties(int follower, TrackProperties& props)
{
    LogicalTrack* lt = trackManager->getLogicalTrack(follower);
    if (lt == nullptr || lt->getSyncSourceNow() != SyncSourceTrack)
      props.invalid = true;
    else {
        // this little dance needs to be encapsulated somewhere, probably Pulsator
        int leader = lt->getSyncLeaderNow();
        if (leader == 0)
          leader = syncMaster->getTrackSyncMaster();
        
        if (leader == 0)
          props.invalid;
        else {
            trackManager->getTrackProperties(leader, props);
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

