/**
 * Montior each realtime event as it comes in and set various flags
 * to indiciate what state we're in.
 *
 * To NOT trace the major Start/Stop/Continue transitions here,
 * just trace anomolies.
 *
 * The only thing obscure here is the orientation of the beat number
 * and beat counter relative to the Song Position when resuming after
 * a Continue.  Continue always starts exactly on a song position, which
 * will be the one we stopped on, or one that was sent while stopped.
 *
 * Beats are 24 clocks and song position "units" are 6 clocks.
 *
 * There are then 4 song position units per beat, so the beat number
 * after Continue is:
 *
 *       SongPosition / 4
 * 
 * Since this rounds down, there can be elapsed SPP units within this beat.
 * The number of those are:
 *
 *       SongPosition % 4
 *
 * Multiply this by 6 to get the beatCounter.  Example: Continue at SPP 6
 * SP6 is within beat 1 (6 / 4 = 1).  Beat one began on SP4 so two SP units
 * have elapsed in this beat (6 % 4 = 2).  At 6 clocks per SP unit  the
 * beat counter is 12 (6 * 2).  So after 12 more clocks we roll to beat 2.
 *
 * We of course don't have to keep two counters for this but is makes the
 * math less obvious.
 *
 */

#include "../util/Trace.h"

#include "MidiEventMonitor.h"

#define MS_START            0xFA
#define MS_CONTINUE         0xFB
#define MS_STOP             0xFC
#define MS_SONGPOSITION		0xF2
#define MS_CLOCK            0xF8

void MidiEventMonitor::reset()
{
    started = false;
    continued = false;
    songPosition = 0;
    beat = 0;
    clock = 0;
    elapsedBeats = 0;
    
    startPending = false;
    continuePending = false;
    songUnitClock = 0;
    beatClock = 0;
}

void MidiEventMonitor::consume(const juce::MidiMessage& msg)
{
    const juce::uint8* data = msg.getRawData();
    const juce::uint8 status = *data;
    
	switch (status) {
		case MS_START: {
            if (!started) {
                startPending = true;
                continuePending = false;
            }
            else {
                Trace(1, "MidiEventMonitor: Redundant Start");
            }
		}
            break;
		case MS_CONTINUE: {
            if (!started) {
                startPending = true;
                continuePending = true;
            }
            else {
                Trace(1, "MidiEventMonitor: Redundant Continue");
            }
		}
            break;
		case MS_STOP: {
            if (started) {
                started = false;
                startPending = false;
                continued = false;
                continuePending = false;
            }
            else {
                Trace(1, "MidiEventMonitor: Redundant Stop");
            }
		}
            break;
		case MS_SONGPOSITION: {
            if (!started) {
                songPosition = msg.getSongPositionPointerMidiBeat();
                // these are not watched as closely by MidiAnalyzer so can trace these
                Trace(2, "MidiAnalyzer: SongPosition %d", songPosition);
            }
            else
              Trace(1, "MidiEventMonitor: Unexpected SongPosition");
		}
            break;
		case MS_CLOCK: {
            if (startPending) {
                started = true;
                startPending = false;
                clock = 0;
                elapsedBeats = 0;
                if (!continuePending) {
                    songPosition = 0;
                    songUnitClock = 0;
                    beat = 0;
                    beatClock = 0;
                    continued = false;
                }
                else {
                    songUnitClock = 0;
                    beat = songPosition / 4;
                    beatClock = (songPosition % 4) * 6;
                    continuePending = false;
                    continued = true;
                }
            }
            else if (started) {
                clock++;
                songUnitClock++;
                if (songUnitClock == 6) {
                    songPosition++;
                    songUnitClock = 0;
                }
                beatClock++;
                if (beatClock == 24) {
                    beat++;
                    beatClock = 0;
                    elapsedBeats++;
                }
            }
            else {
                // clocks may continue being sent after stopping
                // these do not advance the song position
            }
        }
		break;
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
