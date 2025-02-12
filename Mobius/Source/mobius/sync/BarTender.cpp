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
            // another that shuoldn't be used in the UI
            Trace(1, "BarTender::getBeatsPerBar(LogicalTrack) with SyncSourceTrack)");
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
            Trace(1, "BarTender::getBarsPerLoop(LogicalTrack) with SyncSourceTrack");
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

//////////////////////////////////////////////////////////////////////
//
// Unit Lengths
//
// These aren't tehnically part of BarTender's core functions but they
// are closely related and I wanted to put them all in one place rather tha
// apread around SyncMaster which is getting too large.
//
// Still kind of messy and very specific to SyncMaster AutoRecord
//
//////////////////////////////////////////////////////////////////////

/**
 * Return the unit length for one of the SyncSources.
 * This may not be used for SyncSourceTrack.
 * The result is zero for None and Master since Master is variable
 * and self defining.
 * 
 * Zero is possible for MidiAnalyzer if we're before the first beat,
 * so this can't be used for an "am I synced" test.
 */
int BarTender::getUnitLength(SyncSource src)
{
    int length = 0;
    switch (src) {
        case SyncSourceNone:
        case SyncSourceMaster:
            break;
        case SyncSourceTrack:
            Trace(1, "BarTender::getUnitLength(SyncSource) with SyncSourceTrack");
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
int BarTender::getTrackUnitLength(LogicalTrack* track, TrackSyncUnit unit)
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
                Trace(1, "BarTender::getTrackUnitLength Track had no cycles");
                cycles = 1;
            }

            if (unit == TrackUnitCycle)
              length = (props.frames / cycles);

            else if (unit == TrackUnitSubcycle) {
                int subcycles = props.subcycles;
                if (subcycles == 0) {
                    // also not supposed to happen
                    // 4 is the most common, but be consistent with 1
                    Trace(1, "BarTender::getTrackUnitLength Track had no subcycles");
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
int BarTender::getTrackUnitLength2(LogicalTrack* lt, TrackSyncUnit unit)
{
    int length = 0;
    
    MslTrack* innerTrack = lt->getMslTrack();
    if (innerTrack == nullptr) {
        Trace(1, "BarTender::getTrackUnitLength What the hell is this thing?");
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
int BarTender::getLeaderUnitLength(LogicalTrack* follower, TrackSyncUnit unit)
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
int BarTender::getSingleAutoRecordUnitLength(LogicalTrack* track)
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
        switch (src) {
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
            int barMultiplier = getBeatsPerBar(beatSource);
            recordLength = beatLength * barMultiplier;
        }

        // if the syncUnit is Loop, then one more multiple
        if (unit == SyncUnitLoop) {
            int loopMultiplier = getBarsPerLoop(beatSource);
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
int BarTender::getLockUnitLength(LogicalTrack* track)
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
        switch (src) {
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

