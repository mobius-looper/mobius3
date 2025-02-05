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

#include "../../util/Trace.h"

#include "../../model/SessionConstants.h"
#include "../../model/Session.h"
#include "../../model/UIAction.h"
#include "../../model/Query.h"

#include "../track/TrackManager.h"
#include "../track/LogicalTrack.h"
#include "../track/MslTrack.h"
#include "../track/TrackProperties.h"

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

void BarTender::loadSession(Session* s)
{
    session = s;
    cacheSessionParameters();
}

void BarTender::cacheSessionParameters()
{
    setHostBeatsPerBar(session->getInt(SessionHostBeatsPerBar));
    setHostBarsPerLoop(session->getInt(SessionHostBarsPerLoop));
    setHostOverride(session->getBool(SessionHostOverride));

    setMidiBeatsPerBar(session->getInt(SessionMidiBeatsPerBar));
    setMidiBarsPerLoop(session->getInt(SessionMidiBarsPerLoop));
}

/**
 * GlobalReset in effect cancels runtime bindings to the time
 * signature parametres and restores them to those in the Session.
 */
void BarTender::globalReset()
{
    cacheSessionParameters();
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
// Actions and Queries
//
//////////////////////////////////////////////////////////////////////

/**
 * !! todo: Don't have the notion of temporary overrides with GlobalReset
 * revisit
 */
bool BarTender::doAction(UIAction* a)
{
    bool handled = true;

    switch (a->symbol->id) {

        case ParamHostBeatsPerBar:
            setHostBeatsPerBar(a->value);
            break;
            
        case ParamHostBarsPerLoop:
            setHostBarsPerLoop(a->value);
            break;
            
        case ParamHostOverride:
            setHostOverride(a->value != 0);
            break;
            
        case ParamMidiBeatsPerBar:
            setMidiBeatsPerBar(a->value);
            break;
            
        case ParamMidiBarsPerLoop:
            setMidiBarsPerLoop(a->value);
            break;

        default: handled = false;
    }
    return handled;
}

// todo: better range limits

void BarTender::setHostBeatsPerBar(int bpb)
{
    if (bpb > 0)
      hostBeatsPerBar = bpb;
}

void BarTender::setHostBarsPerLoop(int bpl)
{
    if (bpl > 0)
      hostBarsPerLoop = bpl;
}

void BarTender::setHostOverride(bool b)
{
    hostOverride = b;
}

void BarTender::setMidiBeatsPerBar(int bpb)
{
    if (bpb > 0)
      midiBeatsPerBar = bpb;
}

void BarTender::setMidiBarsPerLoop(int bpl)
{
    if (bpl > 0)
      midiBarsPerLoop = bpl;
}

bool BarTender::doQuery(Query* q)
{
    bool handled = true;

    switch (q->symbol->id) {

        case ParamHostBeatsPerBar:
            q->value = hostBeatsPerBar;
            break;
            
        case ParamHostBarsPerLoop:
            q->value = hostBarsPerLoop;
            break;
            
        case ParamHostOverride:
            q->value = hostOverride;
            break;
            
        case ParamMidiBeatsPerBar:
            q->value = midiBeatsPerBar;
            break;
            
        case ParamMidiBarsPerLoop:
            q->value = midiBarsPerLoop;
            break;

        default: handled = false;
    }
    return handled;
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
    if (lt != nullptr) {
        SyncSource src = lt->getSyncSourceNow();
        if (src == SyncSourceTrack) {
            // unclear what this means, it could be the subcycle number from
            // the leader track, but really we shouldn't be trying to display beat/bar counts
            // in the UI if this isn't following somethign with well defined beats
        }
        else {
            beat = getBeat(src);
        }
    }
    return beat;
}

int BarTender::getBeat(SyncSource src)
{
    int beat = 0;
    switch (src) {
        case SyncSourceNone:
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
            Transport* transport = syncMaster->getTransport();
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
            // this method can't be done for TrackSync, needs to be in the context
            // of a LogicalTrack
            Trace(1, "BarTender::getBeat(SyncSource) with SyncSourceTrack");
        }
            break;
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
    if (lt != nullptr) {
        SyncSource src = lt->getSyncSourceNow();
        if (src == SyncSourceTrack) {
            // unclear what this means, it could be the cycle number from
            // the leader track, but really we shouldn't be trying to display beat/bar counts
            // in the UI if this isn't following somethign with well defined beats
        }
        else {
            bar = getBar(src);
        }
    }
    return bar;
}

int BarTender::getBar(SyncSource src)
{
    int bar = 0;
    switch (src) {
        case SyncSourceNone:
            break;

        case SyncSourceMidi: {
            MidiAnalyzer* anal = syncMaster->getMidiAnalyzer();
            int raw = anal->getElapsedBeats();
            if (raw > 0) {
                int bpb = getMidiBeatsPerBar();
                bar = (raw / bpb);

                // this is "elapsed bars"
                // two schools of thought here...this could just increase
                // without end like host does, or it could wrap on barsPerLoop like transport does
                // to show a spinning radar in MidiSyncElement, it needs to wrap
                bar = bar % midiBarsPerLoop;
            }
        }
            break;

        case SyncSourceTransport:
        case SyncSourceMaster: {
            Transport* transport = syncMaster->getTransport();
            bar = transport->getBar();
        }
            break;
                
        case SyncSourceHost: {
            HostAnalyzer* anal = syncMaster->getHostAnalyzer();
            int raw = anal->getElapsedBeats();
            int bpb = getHostBeatsPerBar();
            bar = (raw / bpb);
        }
            break;

        case SyncSourceTrack: {
            Trace(1, "BarTender::getBar(SyncSource) with SyncSourceTrack");
        }
            break;
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
    if (lt != nullptr) {
        SyncSource src = lt->getSyncSourceNow();
        if (src == SyncSourceTrack) {
            // unclear what this means, tracks don't remember how many times they've
            // played a loop
        }
        else {
            loop = getLoop(src);
        }
    }

    return loop;
}

int BarTender::getLoop(SyncSource src)
{
    int loop = 0;
    switch (src) {
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
        case SyncSourceMaster: {
            Transport* transport = syncMaster->getTransport();
            loop = transport->getLoop();
        }
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
            Trace(1, "BarTender::getLoop(SyncSource) with SyncSourceTrack");
        }
            break;
    }
    return loop;
}

/**
 * Punting on track overrides for awhile.
 */
int BarTender::getBeatsPerBar(int trackNumber)
{
    return getBeatsPerBar(trackManager->getLogicalTrack(trackNumber));
}

int BarTender::getBeatsPerBar(LogicalTrack* track)
{
    int bpb = 4;
    if (track != nullptr) {
        SyncSource src = track->getSyncSourceNow();
        if (src == SyncSourceTrack) {
            // work to do
        }
        else {
            bpb = getBeatsPerBar(src);
        }
    }
    
    // since this is commonly used for division,
    // always be sure it has life
    if (bpb <= 0) bpb = 4;
    return bpb;
}

int BarTender::getBeatsPerBar(SyncSource src)
{
    int bpb = 4;
    switch (src) {
        case SyncSourceNone:
            // undefined, leave at 4
            break;

        case SyncSourceMidi:
            bpb = getMidiBeatsPerBar();
            break;
                
        case SyncSourceTransport:
        case SyncSourceMaster:
            bpb = syncMaster->getTransport()->getBeatsPerBar();
            break;
                
        case SyncSourceHost:
            bpb = getHostBeatsPerBar();
            break;

        case SyncSourceTrack: {
            Trace(1, "BarTender::getBeatsPerBar(SyncSource) with SyncSourceTrack");
        }
            break;
    }
    
    // since this is commonly used for division,
    // always be sure it has life
    if (bpb <= 0) bpb = 4;
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
    return getBarsPerLoop(trackManager->getLogicalTrack(trackNumber));
}

int BarTender::getBarsPerLoop(LogicalTrack* track)
{
    int bpl = 1;
    if (track != nullptr) {
        SyncSource src = track->getSyncSourceNow();
        if (src == SyncSourceTrack) {
            // work to do
        }
        else {
            bpl = getBarsPerLoop(src);
        }
    }
    if (bpl < 1) bpl = 1;
    return bpl;
}

int BarTender::getBarsPerLoop(SyncSource src)
{
    int bpl = 1;
    switch (src) {
        case SyncSourceNone:
            break;

        case SyncSourceMidi:
            bpl = getMidiBarsPerLoop();
            break;
                
        case SyncSourceTransport:
        case SyncSourceMaster:
            bpl = syncMaster->getTransport()->getBarsPerLoop();
            break;
                
        case SyncSourceHost:
            bpl = getHostBarsPerLoop();
            break;

        case SyncSourceTrack:
            Trace(1, "BarTender::getBarsPerLoop(SyncSource) with SyncSourceTrack");
            break;
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

void BarTender::getLeaderProperties(LogicalTrack* track, TrackProperties& props)
{
    if (track == nullptr || track->getSyncSourceNow() != SyncSourceTrack) {
        props.invalid = true;
    }
    else {
        // this little dance needs to be encapsulated somewhere, probably Pulsator
        int leader = track->getSyncLeaderNow();
        if (leader == 0)
          leader = syncMaster->getTrackSyncMaster();
        
        if (leader == 0)
          props.invalid;
        else {
            trackManager->getTrackProperties(leader, props);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Unit Lengths
//
// These aren't tehnically part of BarTender's core functions but they
// are closely related and it makes more sense to have them down here
// than up in SyncMaster and bounce back and forth.
//
// !! this is a mess, there are two sets here made at different times
// the newer getBaseRecordUnitLength and getRecordUnitLength are what
// SyncMaster uses for synchronized recordings.  What are the others for?
//
//////////////////////////////////////////////////////////////////////

/**
 * Find the fundamental unit length of a sync source.
 */
int BarTender::getUnitLength(int trackNumber)
{
    return getUnitLength(trackManager->getLogicalTrack(trackNumber));
}

int BarTender::getUnitLength(LogicalTrack* track)
{
    int frames = 0;
    if (track != nullptr) {
        SyncSource src = track->getSyncSourceNow();
        if (src == SyncSourceTrack) {
            frames = getTrackSyncUnitLength(track);
            // more here
        }
        else {
            frames = getUnitLength(src);
        }
    }
    return frames;
}

/**
 * This is different than getBaseRecordUnitLength
 * and I don't remember why.  Merge if possible.
 */
int BarTender::getUnitLength(SyncSource src)
{
    int frames = 0;
    switch (src) {
        case SyncSourceNone: {
            // for the purposes of auto-record, we need to get
            // a tempo from somewhere, OG Mobius had autoRecordTempo
            // until we see a need for something more, let the Transport
            // define this
            frames = syncMaster->getTransport()->getUnitLength();
        }
            break;
            
        case SyncSourceTransport:
            frames = syncMaster->getTransport()->getUnitLength();
            break;
            
        case SyncSourceTrack:
            Trace(1, "BarTender::getUnitLength(SyncSource) with SyncSourceTrack");
            break;
            
        case SyncSourceHost:
            frames = syncMaster->getHostAnalyzer()->getUnitLength();
            break;
            
        case SyncSourceMidi:
            frames = syncMaster->getMidiAnalyzer()->getUnitLength();
            break;
            
        case SyncSourceMaster: {
            // unclear...
            // need to nail down what a track follows if it wants to be
            // the Master but some other track was already the Master
            // it can either fall back to Transport sync or Track sync
            frames = syncMaster->getTransport()->getUnitLength();
        }
            break;
    }
    return frames;
}

/**
 * Determining the unit length for track sync is complicated since
 * we don't have as clearly defined notions of what a "beat" or
 * "time signature" is.
 *
 * A track has a length and may be divided into cycles and subcycles.
 * Cycle is often a "bar" and subcycle is often a "beat", but it's
 * more arbitrary than other sync sources.
 */
int BarTender::getTrackSyncUnitLength(LogicalTrack* track)
{
    (void)track;
    int frames = 0;
    
    // note that the track passed here is the FOLLOWING track,
    // what we need to find is the LEADING track

    int masterNumber = syncMaster->getTrackSyncMaster();
    if (masterNumber > 0) {
        LogicalTrack* master = trackManager->getLogicalTrack(masterNumber);
        if (master != nullptr) {

            // here we need to consule the TrackSyncUnit of the FOLLOWING track
            // punt and assume Loop

            TrackProperties props;
            getLeaderProperties(master, props);
            
            if (!props.invalid) {
                frames = props.frames;
            }
        }
    }
    return frames;
}

/**
 * Return the base unit length of one record unit in samples/frames.
 * Zero is acceptable for MidiAnalyzer if we're before the first beat,
 * so this can't be used for an "am I synced" test.
 * This is not relevant for Track sync.
 */
int BarTender::getBaseRecordUnitLength(SyncSource src)
{
    int length = 0;
    switch (src) {
        case SyncSourceNone:
        case SyncSourceMaster:
        case SyncSourceTrack:
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

/**
 * Return the record unit length for a track.
 * This combines the base unit length from the source with
 * the beat/bar/loop unit settings for this track.
 *
 * I'm not certain if track sync should be handled this way.
 * It is probably better if that is always pulsed like it originally was?
 * That makes AutoRecord impossible though...
 */
int BarTender::getRecordUnitLength(LogicalTrack* lt, SyncSource src)
{
    int length = 0;
    
    if (src == SyncSourceTrack) {
        // this is quite different than the others
        // should just have LT methods for all this?
        int trackSyncMaster = syncMaster->getTrackSyncMaster();
        if (trackSyncMaster) {
            Trace(1, "SyncMaster: Asking about TrackSyncMaster and there is none");
        }
        else {
            LogicalTrack* tsm = trackManager->getLogicalTrack(trackSyncMaster);
            if (tsm == nullptr) {
                Trace(1, "SyncMaster: Invalid TrackSyncMaster number %d", trackSyncMaster);
            }
            else {
                // what we want out of this isn't specific to MSL, but this
                // is where it is packaged atm, really this should just be part of
                // LogicalTrack
                MslTrack* innerTrack = tsm->getMslTrack();
                if (innerTrack == nullptr) {
                    Trace(1, "SyncMaster: What the hell is this?  It isn't a track sync master");
                }
                else {
                    TrackSyncUnit tsu = lt->getTrackSyncUnitNow();
                    switch (tsu) {
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
            }
        }
    }
    else {
        length = getBaseRecordUnitLength(src);
        if (length > 0) {
            SyncUnit unit = lt->getSyncUnitNow();
            if (unit == SyncUnitLoop) {
                length = (length * getBeatsPerBar(lt)) * getBarsPerLoop(lt);
            }
            else if (unit == SyncUnitBar) {
                length = (length * getBeatsPerBar(lt));
            }
        }
    }
    return length;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

