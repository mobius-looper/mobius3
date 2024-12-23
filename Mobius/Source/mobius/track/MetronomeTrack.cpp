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
    (void)q;
    return false;
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
    (void)tstate;
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
}

void MetronomeTrack::doStart(UIAction* a)
{
    (void)a;
    Trace(2, "MetronomeTrack::doStart");
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
    float tempo = (float)(a->value) / 100.0f;
    Trace(2, "MetronomeTrack::doTempo %d", a->value);
    (void)tempo;
}

void MetronomeTrack::doBeatsPerBar(UIAction* a)
{
    (void)a;
    Trace(2, "MetronomeTrack::doBeatsPerBar %d", a->value);
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

void MetronomeTrack::advance(int frames)
{
    (void)frames;
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
int MetronomeTrack::calcTempoLength(float tempo, int bpb)
{
    int sampleRate = manager->getContainer()->getSampleRate();
    int samplesPerBeat = (int)((float)sampleRate / ((float)tempo / 60.0f));
    if (bpb < 1) bpb = 1;
    int samplesPerBar = samplesPerBeat * bpb;
    return samplesPerBar;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
