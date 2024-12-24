/**
 * The metronome track isn't really a functional track but it acts like one and
 * the locations within the track represent beats or bars for the metronome which
 * other tracks can sync with.
 *
 * The "length" of the metronome track will be one "bar" that is determined
 * by the parameter metronomeTempo combined with metronomeBeatsPerBar.  The formula is:
 *
 *   lengthInFrames = 
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/Symbol.h"
#include "../../model/Session.h"
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../../model/MobiusState.h"

#include "../MobiusInterface.h"
#include "../Notification.h"

#include "TrackProperties.h"
#include "TrackManager.h"
#include "LogicalTrack.h"
#include "BaseTrack.h"
#include "MetronomeTrack.h"

MetronomeTrack::MetronomeTrack(TrackManager* tm, LogicalTrack* lt) : BaseTrack(tm, lt)
{
    tempo = 120.0f;
    beatsPerBar = 4;
}

MetronomeTrack::~MetronomeTrack()
{
}

//////////////////////////////////////////////////////////////////////
//
// BaseTrack Implementations
//
//////////////////////////////////////////////////////////////////////

void MetronomeTrack::loadSession(Session::Track* def)
{
    (void)def;

    // todo: get tempo and bpb from here
}

void MetronomeTrack::doAction(UIAction* a)
{
    Symbol* s = a->symbol;

    switch (s->id) {
        case FuncMetronomeStop: doStop(a); break;
        case FuncMetronomeStart: doStart(a); break;
        case ParamMetronomeTempo: doTempo(a); break;
        case ParamMetronomeBeatsPerBar: doBeatsPerBar(a); break;
        default:
            Trace(1, "MetronomeTrack: Unhandled action %s", s->getName());
            break;
    }
}

bool MetronomeTrack::doQuery(Query* q)
{
    bool success = false;
    Symbol* s = q->symbol;

    switch (s->id) {
        case ParamMetronomeTempo: {
            // no floats in Query yet...
            q->value = (int)(tempo * 100.0f);
            success = true;
        }
            break;
            
        case ParamMetronomeBeatsPerBar: {
            q->value = beatsPerBar;
            success = true;
        }
            break;
            
        default:
            Trace(1, "MetronomeTrack: Unhandled query %s", s->getName());
            break;
    }
    return success;
}

void MetronomeTrack::processAudioStream(MobiusAudioStream* stream)
{
    advance(stream->getInterruptFrames());
}

void MetronomeTrack::midiEvent(MidiEvent* e)
{
    (void)e;
}

void MetronomeTrack::getTrackProperties(TrackProperties& props)
{
    (void)props;
}

void MetronomeTrack::trackNotification(NotificationId notification, TrackProperties& props)
{
    (void)notification;
    (void)props;
}

int MetronomeTrack::getGroup()
{
    return 0;
}

bool MetronomeTrack::isFocused()
{
    return false;
}

void MetronomeTrack::refreshPriorityState(MobiusState::Track* tstate)
{
    (void)tstate;
}

void MetronomeTrack::refreshState(MobiusState::Track* tstate)
{
    // don't have "running" mode so it's Reset or Play
    if (running)
      tstate->mode = MobiusState::ModePlay;
    else
      tstate->mode = MobiusState::ModeReset;

    // not sure if this is useful but could be
    tstate->frames = frameLength;
    tstate->frame = playFrame;
    tstate->tempo = tempo;
    tstate->beatsPerBar = beatsPerBar;
    tstate->beat = beat;

    // what IS useful is the beat flags, subcycle on beats
    // and cycle or loop on bar
    tstate->beatLoop = barHit;
    barHit = false;
    tstate->beatSubCycle = beatHit;
    beatHit = false;
}

void MetronomeTrack::dump(StructureDumper& d)
{
    (void)d;
}

class MslTrack* MetronomeTrack::getMslTrack()
{
    return nullptr;
}

//////////////////////////////////////////////////////////////////////
//
// Functions
//
//////////////////////////////////////////////////////////////////////

void MetronomeTrack::doStop(UIAction* a)
{
    (void)a;
    Trace(2, "MetronomeTrack::doStop");
    running = false;
    playFrame = 0;
    beatHit = false;
    barHit = false;
}

void MetronomeTrack::doStart(UIAction* a)
{
    (void)a;
    if (!running) {
        if (frameLength == 0) {
            Trace(1, "MetronomeTrack: Can't run without a tempo");
        }
        else {
            running = true;
            playFrame = 0;
            // flash?
            barHit = true;
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Parameters
//
//////////////////////////////////////////////////////////////////////

/**
 * Don't support floating point values in UIAction so assume the value
 * is x100.
 */
void MetronomeTrack::doTempo(UIAction* a)
{
    Trace(2, "MetronomeTrack::doTempo %d", a->value);
    
    tempo = (float)(a->value) / 100.0f;
    setLength(calcTempoLength(tempo, beatsPerBar));
}

void MetronomeTrack::setLength(int l)
{
    frameLength = l;

    if (beatsPerBar <= 0) {
        // divide by zero hedge, shouldn't happen
        framesPerBeat = 0;
    }
    else {
        framesPerBeat = frameLength / beatsPerBar;
    }
    
    if (running) {
        // so does this try to keep the same relative location
        // or start from the beginning?
        if (frameLength > 0) {
            while (playFrame > frameLength)
              playFrame -= frameLength;
        }
    }
}

void MetronomeTrack::doBeatsPerBar(UIAction* a)
{
    Trace(2, "MetronomeTrack::doBeatsPerBar %d", a->value);

    int bpb = a->value;
    if (bpb <= 0)
      Trace(1, "MetronomeTrack: Invalid beatsPerBar %d", bpb);
    else {
        beatsPerBar = bpb;
        setLength(calcTempoLength(tempo, beatsPerBar));
    }
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

void MetronomeTrack::advance(int frames)
{
    if (running) {
        playFrame += frames;
        if (playFrame > frameLength) {
            barHit = true;
            playFrame -= frameLength;
            if (playFrame > frameLength) {
                // something is off here, must be an extremely short block size
                Trace(1, "MetronomeTrack: PlayFrame anomoly");
            }
        }
        else if (framesPerBeat > 0) {
            int b = playFrame / framesPerBeat;
            if (b != beat) {
                beat = b;
                beatHit = true;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Tempo Math
//
//////////////////////////////////////////////////////////////////////

/**
 * Calculate the tempo relative to another track length.
 *
 * The track is arbitrarily long and represents some number of
 * metronome beats at a tempo.
 *
 * The length of a metronome beats must be an even multiple of the
 * other track length which may have round off errors.  If there is rounding,
 * then the other track should adjust it's length.
 *
 * todo: need a way to convey the adjusted length back to the track
 */
#if 0
float Synchronizer::calcTempo(Loop* l, int beatsPerBar, long frames, 
                                      int* retPulses)
{
	float tempo = 0.0;
	int pulses = 0;

	if (frames > 0) {

        Setup* setup = mMobius->getActiveSetup();
        // SyncState should already have figured out the beat count
        // new: these were unreferenced
        // Track* t = l->getTrack();
        // SyncState* state = t->getSyncState();

        // 24 MIDI clocks per beat
		pulses = beatsPerBar * 24;
		
        // original forumla
        // I don't know how I arrived at this, it works but
        // it is too obscure to explain in the docs
		//float cycleSeconds = (float)frames /  (float)CD_SAMPLE_RATE;
		//tempo = (float)beatsPerBar * (60.0f / cycleSeconds);

        // more obvoius formula
        float framesPerBeat = (float)frames / (float)beatsPerBar;
        float secondsPerBeat = framesPerBeat / (float)mMobius->getSampleRate();
        tempo = 60.0f / secondsPerBeat;

		float original = tempo;
		float fpulses = (float)pulses;

		// guard against extremely low or high values
		// allow these to be floats?
		int min = setup->getMinTempo();
		int max = setup->getMaxTempo();

		if (max < SYNC_MIN_TEMPO || max > SYNC_MAX_TEMPO)
		  max = SYNC_MAX_TEMPO;

		if (min < SYNC_MIN_TEMPO || min > SYNC_MAX_TEMPO)
		  min = SYNC_MIN_TEMPO;

		while (tempo > max) {
			tempo /= 2.0;
			fpulses /= 2.0;
		}

		// if a conflicting min/max specified, min wins
		while (tempo < min) {
			tempo *= 2.0;
			fpulses *= 2.0;
		}
        
        Trace(l, 2, "Sync: calcTempo frames %ld beatsPerBar %ld pulses %ld tempo %ld (x100)\n",
              frames, (long)beatsPerBar, (long)fpulses, (long)(tempo * 100));

		if (tempo != original) {
			Trace(l, 2, "Sync: calcTempo wrapped from %ld to %ld (x100) pulses from %ld to %ld\n",
				  (long)(original * 100),
				  (long)(tempo * 100), 
				  (long)pulses, 
				  (long)fpulses);

            // care about roundoff?
            // !! yes we do...need to have a integral number of pulses and
            // we ordinally will unless beatsPerBar is odd
            float intpart;
            float frac = modff(fpulses, &intpart);
            if (frac != 0) {
                Trace(l, 1, "WARNING: Sync: non-integral master pulse count %ld (x10)\n",
                      (long)(fpulses * 10));
            }
            pulses = (int)fpulses;
		}
	}

	if (retPulses != NULL) *retPulses = pulses;
    return tempo;
}
#endif

/**
 * Calculate the length of this virtual track to make it large enough
 * for the given tempo.
 *
 * At a BPM of 60, there is one beat every second.
 * With a sample rate of 44100 there is one beat every 44100 frames.
 *
 * The length of a bar is determined by multiplying the framesPerBeat by
 * the metronomeBeatsPerBar parameter.
 */
int MetronomeTrack::calcTempoLength(float t, int bpb)
{
    int sampleRate = manager->getContainer()->getSampleRate();
    int samplesPerBeat = (int)((float)sampleRate / ((float)t / 60.0f));
    if (bpb < 1) bpb = 1;
    int samplesPerBar = samplesPerBeat * bpb;
    return samplesPerBar;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
