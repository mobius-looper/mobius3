/**
 * Like other sync sources, the main purpose of the transport is to define a Tempo
 * and a Unit Length.  Tracks that synchronize recording to the Transport will always
 * be an multiple of the unit length and will stay in sync.
 *
 * The transport also holds BeatsPerBar and BarsPerLoop numbers that may be used
 * to control the locations of synchronization points but these do not affect the unit length.
 *
 * Transport tempo will be set on startup to an initial value defined in the Session.
 * The tempo may be changed at any time through one of these methods:
 *
 *     - User explicitly enters a tempo number or uses Tap Tempo in the UI
 *     - A script sets the transportTempo or transportUnitLength parameters
 *     - A TempoFollow is set for the Host or MIDI clocks
 *     - A TransportMaster track is connected
 *
 * The priority of these if they happen in combination needs thought, but in general
 * the tempo is not guaranteed to remain constant and is ususally under direct user control.
 *
 * Since the Transport has no drift, changing the tempo does not impact tracks that had
 * been synchronizing to it.  It will impact future recordings of those tracks and change
 * quantization points however.
 *
 * The Transport has the notion of a "connected" track.  When a track connects,
 * it changes the tempo to match the length of the track.  In the UI this track will
 * be displayed as the "Transport Master".  Once connected the transport will attempt
 * to maintain a tempo compatible with the track if it is rerecorded, or changes it's
 * length in some say such as LoopSwitch, Undo, or Load.
 *
 * Notes on time:
 *
 * 44100    samples (frames) per second
 * 44.10	samples per millisecond
 * .02268	milliseconds per sample
 * 256  	frames per block
 * 5.805	milliseconds per block
 * 172.27	blocks per second
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/SessionConstants.h"
#include "../../model/SyncConstants.h"
#include "../../model/Session.h"
#include "../../model/SyncState.h"
#include "../../model/PriorityState.h"
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../track/TrackProperties.h"

#include "SyncAnalyzerResult.h"
#include "MidiRealizer.h"
#include "SyncMaster.h"

#include "Transport.h"

//////////////////////////////////////////////////////////////////////
//
// Limits
//
//////////////////////////////////////////////////////////////////////

/**
 * The maximum allowed tempo.
 * As the tempo increases, the beat length decreases.
 * 
 * The only hard constraint we have here is that the tempo can't be
 * so fast that it would result in more than one beat pulse per audio
 * block since Pulsator doesn't handle that.
 *
 * With a 44100 rate and 256 blocks, that's 172 blocks per second.
 * One beat per block would be the equivalent of a BPM of 10,320.
 * 
 * This can be configured lower by the user but not higher.
 */
const float TransportMaxTempo = 1000.0f;

/**
 * The minimum tempo needs more thought.
 * As the tempo decreases, the beat length increases.
 * 
 * It would be nice to allow a tempo of zero which would have
 * the effect of stopping the transport.  But that doesn't mean the
 * loop is infinitely long.  It's rather an adjustment to the playback
 * rate of that loop.
 *
 * A tempo of 10 with a sample rate of 44100 results in a beat length
 * of 264,705 frames.
 */
const float TransportMinTempo = 10.0f;

/**
 * The minimum allowable unit length in frames.
 * This should be around the length of one block.
 * Mostly it just needs to be above zero to guard some divideByZero
 * situations.
 */
const int TransportMinUnitLength = 128;

//////////////////////////////////////////////////////////////////////
//
// Initialization
//
//////////////////////////////////////////////////////////////////////

Transport::Transport(SyncMaster* sm)
{
    syncMaster = sm;
    midiRealizer = sm->getMidiRealizer();
    // this will often be wrong, setSampleRate needs to be called
    // after the audio stream is initialized to get the right rate
    sampleRate = 44100;

    // initial time signature
    unitsPerBeat = 1;
    beatsPerBar = 4;
    barsPerLoop = 1;
    
    // start off with a reasonable tempo, this will change
    // soon when the session is loaded
    defaultTempo = 90.0f;
    userSetTempo(defaultTempo);

    //midiRealizer->setTraceEnabled(true);

    testCorrection = false;
}

Transport::~Transport()
{
}

/**
 * Called whenever the sample rate changes.
 * Initialization happens before the audio devices are open so MobiusContainer
 * won't have the right one when we were constructured.  It may also change at any
 * time after initialization if the user fiddles with the audio device configuration.
 *
 * Since this is used for tempo calculations, go through the tempo/length calculations
 * whenever this changes.  This is okay when the system is quiet, but if there are
 * active tracks going and the unitLength changes, all sorts of weird things can
 * happen.  It should be treated like any other tempo/unit length change, any active
 * tracks following the Transport must be disconnected.
 */
void Transport::setSampleRate(int rate)
{
    sampleRate = rate;

    // not a user action, but sort of is because they manually changed
    // the audio interface, might need to streamline the process here
    userSetTempo(tempo);
}

/**
 * The session has a few things that always take effect but a few are considered
 * "defaults" and will not impact the Transport if it is active.
 *
 * This is important because the Session can change for many reasons
 * and we don't want to reconfigure the transport if the intent was not to change
 * the transport.
 *
 * There is a confusing disconnect between "editing the session" and making runtime
 * changes in the UI.  We could consider UI or script changes to be transient
 * and the defaults from the Session will be restored on Global Reset.  This
 * makes sense in particular for Default Tempo since the active transport tempo can
 * be changed for several reasons and we don't want to lose that every time the Session
 * is edited.  For some of the more obscure parameters like MIDI clock control
 * it is less clear.
 *
 * Some options:
 *   - when the Session is edited, it captures the live state of the Transport
 *     and puts that in the Session so that it is saved along with any other changes
 *     and when we get here, it will be the same as it was.  If you do that, then
 *     you need to do this capture on shutdown, similar to how UIConfig works.
 *
 *  - when the Session is edited, keep track of the user touching any of the Transport
 *    parameters and set a modification flag, this is really ugly and error prone
 *
 */
void Transport::loadSession(Session* s)
{
    session = s;
    cacheSessionParameters(false);
}

/**
 * Should be called when a GlobalReset happens.
 * Restore any runtime parameters to the session defaults.
 *
 * Might need options to make these "sticky" and survive a GR.
 */
void Transport::globalReset()
{
    userStop();
    cacheSessionParameters(true);
}

/**
 * This one behaves differently than most because once a master
 * track connects, it determines the tempo and time signature.
 * Normally on globalReset, the master track will be reset too.
 * But for simple session edit/propagation we leave the current
 * values in place if there is a connected track.
 */
void Transport::cacheSessionParameters(bool force)
{
    defaultTempo = (float)(session->getInt(SessionTransportTempo));
    defaultBeatsPerBar = session->getInt(SessionTransportBeatsPerBar);
    defaultBarsPerLoop = session->getInt(SessionTransportBarsPerLoop);

    // correct uninitialized sessions
    if (defaultTempo == 0.0f)
      defaultTempo = 90.0f;
    
    if (defaultBeatsPerBar < 1)
      defaultBeatsPerBar = 4;
    
    if (defaultBarsPerLoop < 1)
      defaultBarsPerLoop = 1;
    
    midiEnabled = session->getBool(SessionTransportMidi);
    sendClocksWhenStopped = session->getBool(SessionTransportClocks);
    manualStart = session->getBool(SessionTransportManualStart);
    metronomeEnabled = session->getBool(SessionTransportMetronome);
    
    int min = session->getInt(SessionTransportMinTempo);
    if (min == 0) min = 30;
    minTempo = (float)min;

    int max = session->getInt(SessionTransportMaxTempo);
    if (max == 0) max = 300;
    maxTempo = (float)max;

    // only if disconnected or doing global reset
    if (force || master == 0) {
        // setting the tempo is more than just slamming a value in
        userSetTempo(defaultTempo);
        beatsPerBar = defaultBeatsPerBar;
        barsPerLoop = defaultBarsPerLoop;
    }

    // reflect midi options
    if (!midiEnabled) {
        midiRealizer->stop();
    }
    else if (sendClocksWhenStopped) {
        if (!started)
          midiRealizer->startClocks();
    }
    else if (!started) {
        midiRealizer->stopSelective(false, true);
    }
}


/**
 * Transport analyzer does not lock since tempo does not fluctuate like MIDI.
 */
void Transport::lock()
{
}

int Transport::getPlayHead()
{
    return unitPlayHead;
}

bool Transport::isManualStart()
{
    return manualStart;
}

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

void Transport::refreshState(SyncState* state)
{
    state->transportTempo = tempo;
    state->transportBeat = beat;
    state->transportBar = bar;
    state->transportLoop = loop;
    state->transportBeatsPerBar = beatsPerBar;
    state->transportBarsPerLoop = barsPerLoop;
    state->transportUnitLength = unitLength;
    state->transportPlayHead = unitPlayHead;
    state->transportStarted = started;

    // todo: paused might be interesting, but won't happen till
    // we get SongPosition

    // metronomeEnable and midiEnable should always track the Session options
    // until they can be controlled from scripts, then we'll need to include them here
    
}

/**
 * Capture the priority state from the transport.
 */
void Transport::refreshPriorityState(PriorityState* state)
{
    // BarTender is letting us own these, which I think makes sense
    // but I guess it could do it as long as we pass back beatsPerBar
    // and barsPerLoop that match
    state->transportBeat = beat;
    state->transportBar = bar;
    state->transportLoop = loop;
}

//////////////////////////////////////////////////////////////////////
//
// Actions and Queries
//
//////////////////////////////////////////////////////////////////////

bool Transport::doAction(UIAction* a)
{
    bool handled = true;

    switch (a->symbol->id) {

        case ParamTransportTempo: {
            // Action doesn't have a way to pass floats right now so the
            // integer value is x100
            
            // !! if the Transport is locked to a Master track, this should be ignored??
            // if you allow tempo to be changed, then the master should be disconnected
            float newTempo = (float)(a->value) / 100.0f;
            userSetTempo(newTempo);
        }
            break;
            
        case ParamTransportLength: {
            // !! if the Transport is locked to a Master track, this should be ignored
            userSetTempoDuration(a->value);
        }
            break;
            
        case ParamTransportBeatsPerBar:
            userSetBeatsPerBar(a->value);
            break;
            
        case ParamTransportBarsPerLoop:
            userSetBarsPerLoop(a->value);
            break;

        case ParamTransportMidi:
            userSetMidiEnabled(a->value != 0);
            break;

        case ParamTransportClocks:
            userSetMidiClocks(a->value != 0);
            break;

        case ParamTransportManualStart:
            manualStart = (a->value != 0);
            break;

        case ParamTransportMinTempo:
            userSetTempoRange(a->value, 0);
            break;

        case ParamTransportMaxTempo:
            userSetTempoRange(0, a->value);
            break;

        case ParamTransportMetronome:
            userSetMetronome(a->value != 0);
            break;
            
        case FuncTransportStop:
            userStop();
            break;

        case FuncTransportStart:
            userStart();
            break;

        case FuncTransportTap:
            userTap();
            break;

        default: handled = false; break;
    }

    return handled;
}

bool Transport::doQuery(Query* q)
{
    bool handled = true;

    switch (q->symbol->id) {
        
        case ParamTransportTempo: {
            // no floats in Query yet...
            q->value = (int)(getTempo() * 100.0f);
        }
            break;
            
        case ParamTransportBeatsPerBar:
            q->value = getBeatsPerBar();
            break;
            
        case ParamTransportBarsPerLoop:
            q->value = getBarsPerLoop();
            break;
            
        case ParamTransportMidi:
            q->value = midiEnabled;
            break;

        case ParamTransportClocks:
            q->value = sendClocksWhenStopped;
            break;

        case ParamTransportManualStart:
            q->value = manualStart;
            break;

        case ParamTransportMinTempo:
            // really need to decide what to do about floats in Query
            q->value = (int)minTempo;
            break;

        case ParamTransportMaxTempo:
            q->value = (int)maxTempo;
            break;

        case ParamTransportMetronome:
            q->value = metronomeEnabled;
            break;

        default: handled = false; break;
    }
    return handled;
}

//////////////////////////////////////////////////////////////////////
//
// SyncAnalyzer Interface
//
// We're not really an "analyzer" we're a source that creates it's own
// reality and self-analyzes.  But need to implement this interface
// for consistency dealing with other sources.
//
//////////////////////////////////////////////////////////////////////

void Transport::analyze(int blockFrames)
{
    advance(blockFrames);
}

SyncAnalyzerResult* Transport::getResult()
{
    return &result;
}

bool Transport::isRunning()
{
    return started;
}

int Transport::getNativeBeat()
{
    return getBeat();
}

int Transport::getNativeBar()
{
    return getBar();
}

int Transport::getElapsedBeats()
{
    // need this?
    return getBeat();
}

int Transport::getNativeBeatsPerBar()
{
    return getBeatsPerBar();
}

float Transport::getTempo()
{
    return tempo;
}

int Transport::getUnitLength()
{
    return unitLength;
}

int Transport::getDrift()
{
    return drifter.getDrift();
}

//////////////////////////////////////////////////////////////////////
//
// Extended Public Interface
//
//////////////////////////////////////////////////////////////////////

int Transport::getBeatsPerBar()
{
    return beatsPerBar;
}

int Transport::getBarsPerLoop()
{
    return barsPerLoop;
}

int Transport::getBeat()
{
    return beat;
}

int Transport::getBar()
{
    return bar;
}

int Transport::getLoop()
{
    return loop;
}

//////////////////////////////////////////////////////////////////////
//
// Manual Control
//
// These are underneath action handlers sent by the UI and provide
// transport control directly to the user rather than automated control
// that happens from within when a master track is connected.
// These also applies to parameters set from scripts.
//
// The UI may choose to prevent manual control when there is currently
// a track connected to the transport.  User commands that change the
// tempo/unit length effectively break the connection between the transport
// and the master track, and disconnect any followers.
//
//////////////////////////////////////////////////////////////////////

/**
 * The user has pressed a "Start" button or taken some other action that
 * is expected to start the transport.
 *
 * If the transport is already started, nothing changes.
 * If the transport is connected to a track and paused, then it will
 * be forcibly resumed and may drift apart from the track.
 *
 * todo: once we allow this, then will probably want various forms of
 * realign to either bring the track into alignment with the transport
 * or move the transport to align with the track.
 */
void Transport::userStart()
{
    start();
}

/**
 * The user has pressed a "Stop" button.
 *
 * Like Start, this yanks control away from the connected track.
 */
void Transport::userStop()
{
    stop();
}

/**
 * The user has requested a time signature change.
 * If the transport is running and has a unit length, this will
 * not change the length of the unit, but will influence the locations
 * of beat and bar pulses.
 *
 * NOTE: If the number is odd, this can result in roundoff errors that cause
 * the final beat to be a different size than the preceeding beats.
 * And similarly if the transport has multiple bars, the final bar may be
 * of a different size than the previous.
 */
void Transport::userSetBeatsPerBar(int bpb)
{
    if (bpb > 0 && bpb != beatsPerBar) {
        Trace(2, "Transport: User changing BeatsPerBar %d", bpb);

        beatsPerBar = bpb;
        recalculateBeats();
    }
}

void Transport::recalculateBeats()
{
    int loopBeats = beatsPerBar * barsPerLoop;
    loop = elapsedUnits / loopBeats;
    int loopRemainder = elapsedUnits % loopBeats;
    bar = loopRemainder / beatsPerBar;
    beat = loopRemainder % beatsPerBar;
}

void Transport::userSetBarsPerLoop(int bpl)
{
    if (bpl > 0 && bpl != barsPerLoop) {
        Trace(2, "Transport: User changing BarsPerLoop %d", bpl);
        
        barsPerLoop = bpl;
        recalculateBeats();
    }
}

void Transport::userSetMidiEnabled(bool b)
{
    midiEnabled = b;
    if (!midiEnabled)
      midiRealizer->stop();
}

void Transport::userSetMidiClocks(bool b)
{
    sendClocksWhenStopped = b;
    if (sendClocksWhenStopped) {
        if (!started)
          midiRealizer->startClocks();
    }
    else if (!started) {
        midiRealizer->stopSelective(false, true);
    }
}

/**
 * This is an action handler so we only need to deal with ints
 * Zero is passed to mean unspecified
 *
 * If we are currently at a tempo that is outside this range, it
 * does not change it.  This is used only for the next tempo
 * derivation.
 */
void Transport::userSetTempoRange(int min, int max)
{
    if (min >= 30)
      minTempo = (float)min;

    if (max > 0 && max <= 300)
      maxTempo = (float)max;
}

/**
 * Turn the metronome on and off with an action.
 * Not implemented yet but will likely be more than just
 * setting a flag.
 */
void Transport::userSetMetronome(bool b)
{
    metronomeEnabled = b;
}

/**
 * Set a tempo specified by the user.
 *
 * There are two ways to do this, with a specific tempo number
 * or with a duration.
 *
 * Using a tempo number would be done when the UI offers a place
 * where a tempo can be entered manually or selected from a menu.
 *
 * Using a duration would be done when the UI provides a "tap tempo"
 * interface where the user pushes a button several times.
 *
 * If the transport is currenty connected to a master track, this
 * will restructure the transport and break the connection.
 */
void Transport::userSetTempo(float newTempo)
{
    if (newTempo >= TransportMinTempo && newTempo <= TransportMaxTempo) {
        deriveUnitLength(newTempo);
        // the master track if any is disconnected
        master = 0;
    }
    else {
        Trace(1, "Transport::userSetTempo Tempo out of range %d", (int)newTempo);
    }
}

/**
 * The tempo is being set using a tap tempo duration
 * in milliseconds.
 */
void Transport::userSetTempoDuration(int millis)
{
    float samplesPerMillisecond = (float)sampleRate / 1000.0f;
    int frames = (int)((float)millis * samplesPerMillisecond);
    if (frames >= TransportMinUnitLength) {
        (void)deriveTempo(frames);
        // the master track if any is disconnected
        master = 0;
    }
    else {
        Trace(1, "Transport::userSetTempoDuration Duration out of range %d", millis);
    }
}

/**
 * Here via the FuncTransportTap function.
 * Tap tempo was originally implemented in the TransportElement which measured the
 * tap distance, calculated a tempo, and then sent that down with an action
 * on ParamTransportTempo.
 *
 * But this really needs to be a bindable function handled down here so the user can
 * have a tap-tempo MIDI button.
 */
void Transport::userTap()
{
    if (tapStart == 0) {
        tapStart = juce::Time::getMillisecondCounter();
    }
    else {
        juce::uint32 tapEnd = juce::Time::getMillisecondCounter();

        bool tempoMethod = false;
        if (tempoMethod) {
            float ftempo = 60000.0f / (float)(tapEnd - tapStart);

            // TransportElement would convert this to a 100x int since
            // UIAction can't do floats and send it down through ParamTransportTempo
            
            /*
            // sigh, UIAtion can't convey a full float yet, have to bump it up
            // and truncate to two decimal places
            int itempo = (int)(ftempo * 100);

            UIAction a;
            a.symbol = provider->getSymbols()->getSymbol(ParamTransportTempo);
            a.value = itempo;
            provider->doAction(&a);
            */
            userSetTempo(ftempo);
        }
        else {
            // length method
            // mostly just for testing, though this might be useful?
            int millis = tapEnd - tapStart;

            // TransportElement would send this down via ParamTransportLength
            /*
            UIAction a;
            a.symbol = provider->getSymbols()->getSymbol(ParamTransportLength);
            a.value = millis;
            provider->doAction(&a);
            */

            // !! if the Transport is locked to a Master track, this should be ignored
            userSetTempoDuration(millis);
        }

        // reset this for next time
        tapStart = 0;
    }
}

//////////////////////////////////////////////////////////////////////
//
// User Defined Tempo Math
//
//////////////////////////////////////////////////////////////////////

/**
 * Calculate the tempo and unit length based on a frame length from the outside.
 *
 * Struggling with options here, but need to guess the user's intent for
 * the length.  The most common use for this is tap tempo, where each tap
 * length represents one beat which becomes the unit length.
 *
 * But they could also be thinking of tapping bars, where the tep length would be divided
 * by beatsPerBar to derive the unit length.
 *
 * Or they could be tapping an entire loop dividied by barsPerLoop (e.g. 12-bar pattern)
 * and beatsPerBar.
 *
 * Without guidance, we would need to guess by seeing which lenght assumption results
 * in a tempo that is closest with the fewest adjustments.
 *
 * Start with simple tempo double/halve and revisit this.
 */
int Transport::deriveTempo(int tapFrames)
{
    int adjust = 0;

    // todo: would we allow setting length to zero to reset the transport?
    if (tapFrames < TransportMinUnitLength) {
        Trace(1, "Transport: Tap frames out of range %d", tapFrames);
    }
    else {
        int newUnitLength = tapFrames;
        float newTempo = lengthToTempo(newUnitLength);

        if (newTempo > maxTempo) {
            while (newTempo > maxTempo) {
                newUnitLength *= 2;
                newTempo = lengthToTempo(newUnitLength);
            }
        }
        else if (newTempo < minTempo) {

            if ((newUnitLength % 2) > 0) {
                Trace(2, "Transport: Rounding odd unit length %d", newUnitLength);
                newUnitLength--;
                adjust = newUnitLength - tapFrames;
            }
            
            while (newTempo < minTempo) {
                newUnitLength /= 2;
                newTempo = lengthToTempo(newUnitLength);
            }
        }

        setTempoInternal(newTempo, newUnitLength);
        // leave BPB and BPL where it is
	}

    return adjust;
}

float Transport::lengthToTempo(int frames)
{
    if (frames == 0) {
        Trace(1, "Transport::lengthToTempo Frames is zero and is angry");
        return 60.0f;
    }
    float secondsPerUnit = (float)frames / (float)sampleRate;
    float newTempo = 60.0f / secondsPerUnit;
    return newTempo;
}

void Transport::setTempoInternal(float newTempo, int newUnitLength)
{
    tempo = newTempo;
    unitLength = newUnitLength;
    // get rid of this if we don't need it
    unitsPerBeat = 1;
    
    // for verification, purposelfy make the tempo we send to the
    // clock generator wrong
    if (testCorrection)
      midiRealizer->setTempo(tempo - 0.1f);
    else
      midiRealizer->setTempo(tempo);
    
    if (midiEnabled && sendClocksWhenStopped)
      midiRealizer->startClocks();
    
    // comments from HostAnalyzer
    //   orient assumes we're exactly on a beat, which is the case if
    //   we're doing tempo derivation by watching beats, but not necessarily
    //   if the user is changing the host tempo while it plays
    //   more to do here
    // For Transport it's going to be more complicated.  MidiRealizer doesn't
    // apply tempo until the next timer thread cycle, may need some handshaking?
    // !! or record the fact that we want to orient, and then orient on the next beat
    // since the reception of the next beat is delayed by at least one block, will need
    // accurate measurements to know where the drifter's playHead location should be
    drifter.orient(unitLength);
    
    // doesn't really matter how large this is
    if (beatsPerBar < 1) {
        Trace(1, "Transport: Correcting mangled beatsPerBar");
        beatsPerBar = 4;
    }

    // if you change tempo while the transport is playing the playHead can be
    // beyond the new unit length and needs to be wrapped
    wrapPlayHead();
}

/**
 * Given the desired tempo, determine the unit lengths.
 * The tempo may be adjusted slightly to allow for integral unitFrames.
 */
void Transport::deriveUnitLength(float newTempo)
{
    // should have caught this by now, how many callers are there?
    // mostly prevent divide by zero below
    
    if (newTempo < TransportMinTempo) {
        Trace(1, "Transport::deriveUnitLength You're doing it wrong");
    }
    else {
        if (newTempo < TransportMinTempo)
          newTempo = TransportMinTempo;
        else if (newTempo > TransportMaxTempo)
          newTempo = TransportMaxTempo;

        float beatsPerSecond = newTempo / 60.0f;
        int framesPerBeat = (int)((float)sampleRate / beatsPerSecond);

        setTempoInternal(newTempo, framesPerBeat);
    }
}

/**
 * After deriving either the tempo or the unit length wrap the playFrame
 * if necessary.
 */
void Transport::wrapPlayHead()
{
    if (unitLength <= 0) {
        Trace(1, "Transport: Wrap with empty unit frames");
    }
    else {
        // playFrame must always be within the unit length,
        // but if we're in a multi-bar loop keep it as high as possible?
        if (unitPlayHead > unitLength) {
            unitPlayHead = (int)(unitPlayHead % unitLength);
            
            // unclear what beat/bar/loop these should mean now
            // changing the unit length doesn't change the relative location
            // within a multi-bar loop so just leave them

            // elapsedUnits might be wrong if that makes a difference

            // unitCounter I think is okay we didn't remove any elapsed units
            // just reoriented the location within the a unit
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Connection
//
// Also knows as "setting the transport master".
//
//////////////////////////////////////////////////////////////////////

/**
 * Connect the transport to a track.
 *
 * This results in a restructuring of the transport to give it a tempo
 * and unitLength that fit with the track contents.
 *
 * Here is where the magic happens.
 *
 * Try to pick the smallest basis that can be an even division of the track length.
 * If the length is short this can be treated like tap tempo.
 * If it is long then we have to guess how many "bars" should be in the track.
 *
 * This needs to be smarter, winging it ATM to get it working.
 *
 * There are many ways these calcultions could be performed, some more elegant
 * than others.  I'm not worried about elegance here, but something that is
 * obvious by reading the algorithm.
 *
 */
void Transport::connect(TrackProperties& props)
{
    if (props.invalid) {
        Trace(1, "Transport: Attempted connection to invalid TrackProperties");
    }
    else if (props.frames == 0) {
        // this is normal?
        // I think no, you should only try to connect after recording of a
        // track that has length, just cursoring over empty tracks that
        // have the potential to be masters does not make them the master
        Trace(2, "Transport: Attempted connection to empty track %d", props.number);
    }
    else if (props.frames < 1000) {
        // while we're here, if this is really short we're going to spin trying
        // to get the tempo in range, this is probably an error
        Trace(1, "Transport: Attempt to connect to an extremly short track");
    }
    else {

        // if another track is currently connected, disconnect it
        if (master > 0 && master != props.number)
          disconnect();
    
        int newUnitLength = props.frames;
        int resultBars = 1;

        // let's get this out of the way early
        // if the number of frames in the loop is not even, then all sorts of
        // assumptions get messy, this should have been prevented by now
        // if the number of cycles and bpb is also odd, this might result in an acceptable
        // unit, but it is sure to cause problems down the road
        if ((newUnitLength % 2) != 0) {
            Trace(1, "Transport::connect Uneven loop frames %d, this will probably suck",
                  newUnitLength);
        }

        // try to divide by cycles it if is clean
        if (props.cycles > 1) {
            int cycleFrames = newUnitLength / props.cycles;
            int expectedFrames = cycleFrames * props.cycles;
            if (expectedFrames == newUnitLength) {
                // the loop divides cleanly by cycle, the cycle
                // can be the base length
                newUnitLength = cycleFrames;
                resultBars = props.cycles;
            }
            else {
                // some number was odd in the loop's final length calculation
                // the last cycle will not be the same size as the others
                // the truncated cycle length can't be used as a reliable basis
                // this isn't supposed to happen if SyncMaster::requestRecordStop did it's job
                // but it could happen when loading random loops, or the user has taken
                // manual control over the cycle count
                // need more intelligence here
                Trace(1, "Transport: Warning: Inconsistent cycle lengths calculating base unit");
            }
        }
    
        // try to apply the user selected beatsPerBar
        if (beatsPerBar > 1) {
            int beatFrames = newUnitLength / beatsPerBar;
            int expectedFrames = beatFrames * beatsPerBar;
            if (expectedFrames == newUnitLength) {
                // it divides cleanly on beats
                newUnitLength = beatFrames;
            }
            else {
                // not unexpected if they're Brubeking with bpb=5
                // this is where we should have tried to round off the ending
                // of the initial recording so it would divide clean
                // this can't be the unit without another layer of calculations
                // to deal with shortfalls and remainders
                Trace(2, "Warning: Requested Beats Per Bar %d does not like math",
                      beatsPerBar);
            }
        }
    
        // before we start looping should have caught this but be extra safe
        if (newUnitLength < 1) {
            Trace(1, "Transport: Unit frames reached the singularity");
            return;
        }
    
        // start with the usual double/halve approach to get the tempo in range
        // it could be a lot smarter here about dividing long loops into "bars"
        // rather than just assuming a backing pattern is 1,2,4,8,16 bars
        // for example if they're syncing to a 12-bar pattern and the recorded an
        // entire 12 bar loop, then we could know that
        // can't guess though without input, would need a barsPerLoop option as
        // well as beatsPerBar and barsPerLoop takes the place of the cycle count
        // if the loop wasn't already divided into cycles, that might be the easiest way

        float newTempo = lengthToTempo(newUnitLength);

        if (newTempo > maxTempo) {
            // the loop is very short, not uncommon if it was recorded like "tap tempo"
            // and intended to be a beat length rather than a bar length
            // we could abandon BeatsParBar at this point and just find some beat subdivision
            // that results in a usable tempo but try to do what they asked for

            while (newTempo > maxTempo) {
                newUnitLength *= 2;
                newTempo = lengthToTempo(newUnitLength);
            }
        }
        else if (newTempo < minTempo) {
            // the loop is very long
            // if the unit frames is uneven, then this will suck
            // should have been a forced adjustment in notifyTrackRecordEnding
            if ((newUnitLength % 2) > 0) {
                Trace(1, "Transport: Rounding odd unit length %d", newUnitLength);
                newUnitLength--;
                // adjust = newUnitLength - tapFrames;
            }

            while (newTempo < minTempo) {
                newUnitLength /= 2;
                if (newUnitLength < 2) {
                    Trace(1, "Transport: Unit frames reached the singularity");
                    return;
                }
                newTempo = lengthToTempo(newUnitLength);
            }
        }

        // at this point a unit is a "beat" and can calculate how many bars are in the
        // resulting loop
        if (beatsPerBar < 1) {
            Trace(1, "Transport: Correcting mangled beatsPerBar");
            beatsPerBar = 4;
        }
        int barFrames = newUnitLength * beatsPerBar;
        int bars = props.frames / barFrames;
        int expectedFrames = bars * barFrames;
        if (expectedFrames != props.frames) {
            // roundoff error, could have used ceil() here
            bars++;
        }
        barsPerLoop = bars;

        // now we have location
        // Connection usually happens when the loop is at the beginning, but it
        // can also happen randomly
        // until we support SongPosition, connection only sets the tempo and relies
        // on Realign to bring either side into alignment
        // old Mobius had various options for this, need to break away from EDP-ness
        // and make this far more flexible
        //
        // All setTempoInternal does is wrap the playHead in case it is currently
        // beyond the new unitLength
        setTempoInternal(newTempo, newUnitLength);
        master = props.number;

        doConnectionActions();
    }
}

/**
 * After a track has successfully connected as the master and adjusted the
 * tempo and unit length, we can do various things to the transport play head
 * and generated MIDI.
 *
 * The most obvioius is to send MS_START clocks.  Old Mobius had some options
 * here around "manual start" that need to be restored.
 *
 * SyncMaster is also doing things around this that need to be moved down here,
 * Transport should be the only thing decididing the fate of MidiRealizer.
 */
void Transport::doConnectionActions()
{
    // if MIDI is enabled and clocks are not being sent, AND the master is
    // at the start point, send MIDI start

    // !! more to do here
    if (!started) {
        Trace(2, "Transport: Master track connected, sending start");
        Trace(2, "Transport: Should be checking ManualStart");
        start();
    }
}


/**
 * The Master is in current practice a track number and having
 * a non-zero value means this track is the TransportMaster.
 *
 * When we get the point of implementing Tempo Lock to the Host or MIDI,
 * This could either be a special Connection number or something else.
 */
int Transport::getMaster()
{
    return master;
}

/**
 * Disconnect the transport from a track.
 * 
 * This has no effect other than clearing the connection number.
 * Might want to have side effects here, like stopping clocks, but we are often
 * also in the process of reconnecting to a different track so defer that.
 *
 * If we need to support "disconnect without assigning a new master" then there
 * should be a public disconnect() for that purpose and an internalDisconnect()
 * that has fewer side effects.
 */
void Transport::disconnect()
{
    master = 0;
}

//////////////////////////////////////////////////////////////////////
//
// Internal Transport Controls
//
//////////////////////////////////////////////////////////////////////

void Transport::resetLocation()
{
    unitPlayHead = 0;
    elapsedUnits = 0;
    unitCounter = 0;
    beat = 0;
    bar = 0;
    loop = 0;
}

/**
 * Complex issues here related to sync pulses.
 * Block processing proceeds in three phases:
 *
 *     1) SyncMaster::beginAudioBlock
 *        - this is where sync sources are advanced and Pulsator gathers pulses
 *
 *     2) MobiusKernel processes queued actions
 *        - this is where the Transport can be started, stopped, or moved
 *
 *     3) SyncMaster::processAudioStream
 *        - this is where TimeSlicer looks at Pulses and sends them to the tracks
 *
 * Most sync sources are external and can't be under the control of a script or user action.
 * Those can be fully analyzed in phase 1.
 *
 * Transport is different because it can be changed after phase 1, and this could result in
 * pulse events being detected after the initial advance.  In particular this happens
 * when an action calls Transport::start, in theory there could be other actions though
 * none are as important.
 *
 * Whenever the Transport is touched in such a way that the pulse analysis changes, it must
 * notify SyncMaster so the block Pulse held by Pulsator is refreshed.
 *
 * Eventually scripts may introduce even more complications to this.  Example:
 *
 *     track 1 enters Synchronize mode waiting for a Transport pulse
 *     track 2 resumes a script that calls TransportStart
 *
 * Even if the Transport notifies SyncMaster abhout the new pulse, track 1 may already
 * have been advanced for this block and will miss the pulse.  There is no carryover
 * of pulses from one block to the next and TimeSlicer does not order tracks based on
 * potential dependencies that come from scripts.
 *
 * Pulse carryover is possible, but not accurate since the dependent track will receive
 * the pulse on the next block which could lead to minor timing errors.
 *
 * Punting on this for the time being since script usage is relatively simple and does not
 * often control the Transport.
 *
 * Complication 2: Play head advance after start
 *
 * A more serious complication that must be dealt with is where the unitPlayHead goes
 * after the start.  Since start() happens in the Action phase, Transport::advance() has
 * already been called and the unitPlayHead was advanced for this block.   start() sets the
 * head back to zero, but the playHead position is actually the number of frames that
 * remain in "block time" after the start() happened.  Failure to do this will result
 * in up to one extra block being added to the width of the first sync unit after the start().
 * SyncMaster provides a "block time" call to get the offset within the block where
 * the start() action is happening.  
 */
void Transport::start()
{
    if (!started) {
        started = true;
        Trace(2, "Transport: Starting unitPlayHead %d unitLength %d",
              unitPlayHead, unitLength);

        // this may be considered a beat pulse in case a track is stuck in Synchronize
        // waiting for the transport to start back up
        // !! testing for unitPlayHead zero catches the most common case, but in theory
        // the playHead could be over any location that would result in a beat within
        // this block after the transport is resumed
        if (unitPlayHead == 0) {
            // set flags in the SyncAnalyzerResult indiciating this is a beat and ask
            // SyncMaster to regenerate the Pulse
            result.started = true;
            result.beatDetected = true;
            result.barDetected = true;
            result.loopDetected = true;

            result.blockOffset = syncMaster->getBlockOffset();

            int blockSize = syncMaster->getBlockSize();
            unitPlayHead = blockSize - result.blockOffset;
            
            syncMaster->notifyTransportStarted();
        }
        else {
            // advance from where we are now which may detect a beat
            detectPostStartBeat();
        }
        
        // going to need a lot more state here
        if (midiEnabled) {
            // We're normally in a UIAction handler at this point before
            // MobiusKernel advances SyncMaster.  MS_START and clocks will begin on
            // the next timer thread cycle, but even if that happens soon, MidiRealizer
            // may have captured the queue early.  The end result is that we won't see
            // any events in the queue until the next block.  DriftMonitor needs to
            // be reoriented when the started event comes in, but it can't hurt to do
            // it now, and helps measure initial lag.
            midiRealizer->start();
            drifter.orient(unitLength);
        }
    }
}

/**
 * To detect beats after a delayed start, it is enough just to advance the transport.
 * The remainder of the block.
 */
void Transport::detectPostStartBeat()
{
    int offset = syncMaster->getBlockOffset();
    int remaining = syncMaster->getBlockSize() - offset;

    softAdvance(remaining, false);
    syncMaster->notifyTransportStarted();

    // todo: I think there is more to do here with MidiRealizer since we
    // bypassed consumeMidiBeats in softAdvance, needs thought..
    // also a host of issues related to deferring MIDI continue until the transport
    // is exactly on a SongPosition
}

void Transport::startClocks()
{
    // in theory could be watching drift now too, but
    // wait until start
    if (midiEnabled)
      midiRealizer->startClocks();
}

void Transport::stop()
{
    pause();
    resetLocation();
}

void Transport::stopSelective(bool sendStop, bool stopClocks)
{
    Trace(1, "Transport::stopSelective");
    (void)sendStop;
    (void)stopClocks;
}

void Transport::pause()
{
    if (midiEnabled) {
        if (sendClocksWhenStopped)
          midiRealizer->stopSelective(true, false);
        else
          midiRealizer->stop();
    }
    
    started = false;
}

void Transport::resume()
{
    // todo: a lot more with song clocks
    start();
}

bool Transport::isPaused()
{
    return paused;
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * Advance the transport and detect whether a beat pulse was encountered.
 */
void Transport::advance(int frames)
{
    result.reset();
    softAdvance(frames, true);
}

/**
 * Core advance logic that doees not reset the result so
 * we can set initial beat flags after start()
 */
void Transport::softAdvance(int frames, bool doMidi)
{
    if (started) {

        unitPlayHead = unitPlayHead + frames;
        if (unitPlayHead >= unitLength) {
            // a unit has transpired
            int over = unitPlayHead - unitLength;
            if (over > frames || over < 0) {
                // can happen with MIDI when suspended in the debugger, shouldn't here
                Trace(1, "Transport: You suck at math");
                over = 0;
            }
            
            int blockOffset = frames - over;
            unitPlayHead = over;

            elapsedUnits++;
            unitCounter++;

            if (unitCounter >= unitsPerBeat) {

                unitCounter = 0;
                beat++;
                result.beatDetected = true;
                result.blockOffset = blockOffset;

                if (beat >= beatsPerBar) {

                    beat = 0;
                    bar++;
                    result.barDetected = true;

                    if (bar >= barsPerLoop) {

                        bar = 0;
                        loop++;
                        result.loopDetected = true;
                    }
                }
            }
        }

        // also advance the drift monitor
        // doMidi set only on the initial advance, not a soft advance
        // after inner block start()
        if (doMidi && midiEnabled) {
            // HostAnalyzer did PPQ first but I don't think order matters
            consumeMidiBeats();
            drifter.advanceStreamTime(frames);
            
        }
    }

    if (doMidi && result.loopDetected && midiEnabled)
      checkDrift();
}

//////////////////////////////////////////////////////////////////////
//
// Midi Event Analysis
//
//////////////////////////////////////////////////////////////////////

/**
 * MidiRealizer behaves much like MidiAnalyzer and generates a
 * SyncAnalyzerResult at the beginning of each block.
 * SyncMaster will have advanced it before Transport, so we can
 * look at it's result for happenings.
 *
 * The logic here is similar to what Pulsator::convertPulse does
 * except we only care about beats and not stops and starts.
 *
 * !! Something is off here but I don't know what yet...
 *
 * Without the reorientation on Start, this starts out with a rather
 * large negative drift of around -29xxx but then stays pretty constant.
 * With the reorientation on Start, drift hovers around here:
 *
 * Transport: Drift 192
 * Transport: Drift 192
 * Transport: Drift 192
 * Transport: Drift -64
 * Transport: Drift 192
 * Transport: Drift 192
 *
 * This may be due to the blockOffset error which is not being handled
 * correctly yet, which makes sense since the amounts are less than a block size.
 *
 * I'm surpised my initial lag trace didn't come out though, would have
 * expected that 29xxx number to be there.
 *
 * The good news is that drift seems to be staying constant enough not
 * to worry about for awhile, but need to revisit this.
 */
void Transport::consumeMidiBeats()
{
    SyncAnalyzerResult* midiResult = midiRealizer->getResult();
    if (midiResult != nullptr) {
        if (midiResult->beatDetected) {

            if (midiResult->started) {
                // MidiRealizer got around to sending the MS_START
                // and will now start with clocks
                // resync the drift monitor

                // Curious about what the lag was
                int lag = drifter.getStreamTime();
                drifter.orient(unitLength);
                if (lag > 0)
                  Trace(2, "Transport: Initial MIDI clock lag %d", lag);
            }
            else {
                drifter.addBeat(midiResult->blockOffset);
            }
        }
    }
}

void Transport::checkDrift()
{
    int drift = drifter.getDrift();
    if (drift > 256)
      Trace(2, "Transport: Drift %d", drift);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

