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

#include "Pulse.h"
#include "HostAnalyzer.h"
#include "SyncSourceResult.h"
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
    SyncSourceResult* result = anal->getResult();
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

Pulse* BarTender::annotate(Follower* f, Pulse* src)
{
    (void)f;
    return src;
}

//////////////////////////////////////////////////////////////////////
//
// Normalized Beats
//
//////////////////////////////////////////////////////////////////////

int BarTender::getBeat(int trackNumber)
{
    (void)trackNumber;
    return 0;
}

int BarTender::getBar(int trackNumber)
{
    (void)trackNumber;
    return 0;
}

int BarTender::getBeatsPerBar(int trackNumber)
{
    (void)trackNumber;
    return 4;
}

int BarTender::getBarsPerLoop(int trackNumber)
{
    (void)trackNumber;
    return 1;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

