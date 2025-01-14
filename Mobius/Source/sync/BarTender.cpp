/**
 * The general control flow for SyncSources is this:
 * 
 * 1) An Analyzer advances and detects beats, Start/Stop, SongPosition.
 *    The Analyzer results are left in a SyncSourceResult.
 *
 * 2) Pulsator advances and converts the SyncSourceResults from all sources
 *    into a Pulse for each source.
 *
 * 3) Pulsator gives the Pulse to BarTender for bar derivation and annotation of the Pulse.
 *
 * !! This doesn't do anything beyond what divide and modulo can do so if this is all
 * it ever is, we don't need the added complexity.  All it really does is save some repetative
 * math.  
 *
 */

#include <JuceHeader.h>

#include "../util/Trace.h"

#include "Pulse.h"
#include "SyncMaster.h"
#include "BarTender.h"

/**
 * Transport will have one of these and it isn't a track, so use the
 * convention that number 0 means the Transport.
 */
void BarTender::setNumber(int n)
{
    number = n;
}

void BarTender::setBeatsPerBar(int n)
{
    if (beatsPerBar != n) {
        beatsPerBar = n;
        reconfigure();
    }
}

void BarTender::setBarsPerLoop(int n)
{
    if (barsPerLoop != n) {
        barsPerLoop = n;
        reconfigure();
    }
}

/**
 * Called when the SyncSource changes the current native beat location.
 * This is only applicable for SyncSourceHost
 */
void BarTender::orient(int newRawBeat)
{
    rawBeat = newRawBeat;
    reconfigure();
}

/**
 * Called when the session is edited or a user action changes either
 * of the beatsPerBar or barsPerLoop values.
 */
void BarTender::reconfigure()
{
    beat = rawBeat;
    bar = 0;
    loop = 0;
    
    if (beatsPerBar > 0) {
        
        beat = rawBeat % beatsPerBar;
        bar = rawBeat / beatsPerBar;
   
        if (barsPerLoop > 1) {
            int rawBar = bar;
            bar = rawBar % barsPerLoop;
            loop = rawBar / barsPerLoop;
        }
    }
}

void BarTender::annotate(Pulse* pulse)
{
    rawBeat++;
    beat++;
                
    if (beatsPerBar > 0 && beat >= beatsPerBar) {

        beat = 0;
        bar++;

        pulse->unit = SyncUnitBar;

        if (barsPerLoop > 1) {

            if (bar >= barsPerLoop) {

                bar = 0;
                loop++;

                pulse->unit = SyncUnitLoop;
            }
        }
    }
}

int BarTender::getBeat()
{
    return beat;
}

int BarTender::getBar()
{
    return bar;
}

int BarTender::getBeatsPerBar()
{
    return beatsPerBar;
}

int BarTender::getBarsPerLoop()
{
    return barsPerLoop;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

