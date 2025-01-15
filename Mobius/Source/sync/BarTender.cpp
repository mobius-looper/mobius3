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
#include "../mobius/track/TrackProperties.h"

#include "Pulse.h"
#include "Follower.h"
#include "Transport.h"
#include "HostAnalyzer.h"
#include "MidiAnalyzer.h"
#include "SyncAnalyzerResult.h"
#include "SyncMaster.h"

#include "BarTender.h"

BarTender::BarTender(SyncMaster* sm)
{
    syncMaster = sm;
}

BarTender::~BarTender()
{
}

/**
 * Problem 1: Where is the default beatsPerBar defined?
 *
 * The Session has two parameters related to time sigatures:
 *
 *     SessionBeatsPerBar
 *     SessionHostOverrideTimeSignature
 *
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
    // save these
    sessionBeatsPerBar = s->getInt(SessionBeatsPerBar);
    sessionHostOverride = s->getBool(SessionHostOverrideTimeSignature);
}

/**
 * Called by SyncMaster when it receives a UIAction to change
 * the transport time signature.
 * Like loadSession, could use this to recalculate track normalized beats.
 */
void BarTender::updateBeatsPerBar(int bpb)
{
    sessionBeatsPerBar = bpb;
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

Pulse* BarTender::annotate(Follower* f, Pulse* beatPulse)
{
    Pulse* result = beatPulse;
    Transport* transport = syncMaster->getTransport();
    bool onBar = false;
    
    switch (f->source) {
        case SyncSourceNone:
            // shouldn't be here
            break;

        case SyncSourceMidi: {
            // it would actually be nice to have the Analyzer return
            // the elapsed beat count which would then be saved in the Pulse
            // so we don't have to go back there to get it
            MidiAnalyzer* anal = syncMaster->getMidiAnalyzer();
            int raw = anal->getElapsedBeats();
            int bpb = transport->getBeatsPerBar();
            onBar = ((raw % bpb) == 0);
        }
            break;

        case SyncSourceTransport:
        case SyncSourceMaster: {
            // Transport did the work for us
            SyncAnalyzerResult* res =  transport->getResult();
            onBar = res->barDetected;
        }
            break;

        case SyncSourceHost: {
            // armegeddon
            onBar = detectHostBar();
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

    if (onBar) {
        // copy the original pulse and change it's unit
        annotated = *beatPulse;
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
bool BarTender::detectHostBar()
{
    bool onBar = false;

    int bpb = getHostBeatsPerBar();
    HostAnalyzer* anal = syncMaster->getHostAnalyzer();

    // here we have the option of basing this on the elapsed beat count
    // or the native beat number, same for getBeat below
    int raw = anal->getNativeBeat();

    onBar = ((raw % bpb) == 0);

    return onBar;
}

//////////////////////////////////////////////////////////////////////
//
// Normalized Beats
//
//////////////////////////////////////////////////////////////////////

int BarTender::getBeat(int trackNumber)
{
    Follower* f = syncMaster->getFollower(trackNumber);
    return getBeat(f);
}

/**
 * Should be maintaining these on each advance, watching for sync pulses
 * for each track and advancing our own counters in Track.  But until then
 * just math the damn things every time.
 */
int BarTender::getBeat(Follower* f)
{
    int beat = 0;
    
    Transport* transport = syncMaster->getTransport();
    
    if (f != nullptr) {
        switch (f->source) {
            case SyncSourceNone:
                // technically should return zero?
                break;

            case SyncSourceMidi: {
                MidiAnalyzer* anal = syncMaster->getMidiAnalyzer();
                int raw = anal->getElapsedBeats();
                if (raw > 0) {
                    // really not liking assumign transport controls this
                    int bpb = transport->getBeatsPerBar();
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
    Follower* f = syncMaster->getFollower(trackNumber);
    return getBar(f);
}

int BarTender::getBar(Follower* f)
{
    int bar = 0;
    
    Transport* transport = syncMaster->getTransport();
    
    if (f != nullptr) {
        switch (f->source) {
            case SyncSourceNone:
                // technically should return zero?
                break;

            case SyncSourceMidi: {
                MidiAnalyzer* anal = syncMaster->getMidiAnalyzer();
                int raw = anal->getElapsedBeats();
                if (raw > 0) {
                    int bpb = transport->getBeatsPerBar();
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

/**
 * Punting on track overrides for awhile.
 */
int BarTender::getBeatsPerBar(int trackNumber)
{
    int bpb = 4;
    
    Follower* f = syncMaster->getFollower(trackNumber);
    if (f != nullptr) {
        switch (f->source) {
            case SyncSourceNone:
                // technically should return zero?
                break;

            case SyncSourceMidi:
                // no midi specific override yet, but may want one
                // fall back to transport
                
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

int BarTender::getHostBeatsPerBar()
{
    int bpb = 4;
    if (sessionHostOverride) {
        // use Transport, probably want a specific setting
        // just for Host so Transport can be independent of whatever
        // they're trying to do with the Host
        bpb = syncMaster->getTransport()->getBeatsPerBar();
    }
    else {
        HostAnalyzer* anal = syncMaster->getHostAnalyzer();
        if (anal->hasNativeTimeSignature()) {
            bpb = anal->getNativeBeatsPerBar();
        }
        else {
            // transport I guess
            bpb = syncMaster->getTransport()->getBeatsPerBar();
        }
    }

    return bpb;
}

/**
 * Kind of a weird one.  Mostly for Transport.
 * For leaders, I guess return the cycle count, though
 * getBeatsPerBar with a sync leader doesn't normally return
 * the leader's subcycle count.
 */
int BarTender::getBarsPerLoop(int trackNumber)
{
    int bpl = 1;
    
    SyncSource source = getSyncSource(trackNumber);
    if (source == SyncSourceTransport) {
        bpl = syncMaster->getTransport()->getBarsPerLoop();
    }
    else if (source == SyncSourceTrack) {
        TrackProperties props;
        getLeaderProperties(trackNumber, props);
        if (!props.invalid && props.cycles > 0)
          bpl = props.cycles;
    }
    
    return bpl;
}

SyncSource BarTender::getSyncSource(int trackNumber)
{
    SyncSource source = SyncSourceNone;
    Follower* f = syncMaster->getFollower(trackNumber);
    if (f != nullptr)
      source = f->source;
    return source;
}

void BarTender::getLeaderProperties(int follower, TrackProperties& props)
{
    Follower* f = syncMaster->getFollower(follower);
    if (f == nullptr || f->source != SyncSourceTrack)
      props.invalid = true;
    else {
        // this little dance needs to be encapsulated somewhere, probably Pulsator
        int leader = f->leader;
        if (leader == 0)
          leader = syncMaster->getTrackSyncMaster();
        
        if (leader == 0)
          props.invalid;
        else {
            // an unusual dependency
            TrackManager* tm = syncMaster->getTrackManager();
            tm->getTrackProperties(leader, props);
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

