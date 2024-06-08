/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Sync control functions.
 * This doesn't actually touch the Synchronizer but goes through a few
 * protected methods in Mobius.  Might be better to get Synchronizer directly.
 *
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "../Action.h"
#include "../Event.h"
#include "../Function.h"
#include "../Loop.h"
#include "../Mobius.h"
#include "../Mode.h"
#include "../Synchronizer.h"
#include "../Track.h"

//////////////////////////////////////////////////////////////////////
//
// SyncEvent
//
//////////////////////////////////////////////////////////////////////

/**
 * Pseudo-events generated by Synchronizer when one the synchronizationm
 * sources (host, midi, timer) has a "pulse".  For MIDI in/out these
 * are also used for start/stop/continue messages.
 *
 * Formerly use the name "SyncEventType" but that is now the name
 * of an enumeration in Event.h.
 *
 * NOTE: This really has nothing to do with the Function and MobiusMode
 * definitions in this file.  Should we move this?
 */
class SynchronizerEventType : public EventType {
  public:
    virtual ~SynchronizerEventType() {}
	SynchronizerEventType();
	void invoke(Loop* l, Event* e);
};

SynchronizerEventType::SynchronizerEventType()
{
	name = "Sync";
}

void SynchronizerEventType::invoke(Loop* l, Event* e)
{
    Synchronizer* sync = l->getSynchronizer();
	sync->syncEvent(l, e);
}

SynchronizerEventType SyncEventObj;
EventType* SyncEvent = &SyncEventObj;

//////////////////////////////////////////////////////////////////////
//
// SyncMaster Modes
//
//////////////////////////////////////////////////////////////////////

//
// SyncMaster
//

class SyncMasterModeType : public MobiusMode {
  public:
	SyncMasterModeType();
};

SyncMasterModeType::SyncMasterModeType() :
    MobiusMode("master", "Master")
{
	minor = true;
}

SyncMasterModeType SyncMasterModeObj;
MobiusMode* SyncMasterMode = &SyncMasterModeObj;

//
// TrackSyncMaster
//

class TrackSyncMasterModeType : public MobiusMode {
  public:
	TrackSyncMasterModeType();
};

TrackSyncMasterModeType::TrackSyncMasterModeType() :
    MobiusMode("trackMaster", "Track Master")
{
	minor = true;
}

TrackSyncMasterModeType TrackSyncMasterModeObj;
MobiusMode* TrackSyncMasterMode = &TrackSyncMasterModeObj;

//
// MIDISyncMaster
//

class MIDISyncMasterModeType : public MobiusMode {
  public:
	MIDISyncMasterModeType();
};

MIDISyncMasterModeType::MIDISyncMasterModeType() :
    MobiusMode("midiMaster", "MIDI Master")
{
	minor = true;
}

MIDISyncMasterModeType MIDISyncMasterModeObj;
MobiusMode* MIDISyncMasterMode = &MIDISyncMasterModeObj;

//////////////////////////////////////////////////////////////////////
//
// SyncMaster Functions
//
//////////////////////////////////////////////////////////////////////

class SyncMasterFunction : public Function {
  public:
	SyncMasterFunction(bool track, bool midi);
	void invoke(Action* action, Mobius* m);
  private:
	bool mTrack;
	bool mMidi;
};

SyncMasterFunction SyncMasterObj {true, true};
Function* SyncMaster = &SyncMasterObj;

SyncMasterFunction SyncMasterTrackObj {true, false};
Function* SyncMasterTrack = &SyncMasterTrackObj;

SyncMasterFunction SyncMasterMidiObj {false, true};
Function* SyncMasterMidi = &SyncMasterMidiObj;

SyncMasterFunction::SyncMasterFunction(bool track, bool midi)
{
	// !! may want to schedule this for MIDI sync???
	global = true;
	mTrack = track;
	mMidi = midi;
    noFocusLock = true;

	if (track && !midi) {
		setName("SyncMasterTrack");
        // why were the others scriptOnly but not this one?
        // Bert noticed this, apparently it is useful to bind a function
        // to this, not sure why.  If we're going to expose this one,
        // why not SyncMasterMidi?
        //scriptOnly = true;
	}
	else if (midi && !track) {
		setName("SyncMasterMidi");
        // doesn't work yet so keep it hidden
        scriptOnly = true;
	}
	else {
		setName("SyncMaster");
        // doesn't work yet so keep it hidden
        scriptOnly = true;
	}
}

/**
 * Changin the track sync master is relatively harmless though
 * if you do this while another track is slave syncing there could
 * strange results.  
 *
 * Changing the out sync master track is quite complicated, 
 * !! think about the latency consequences.
 */
void SyncMasterFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {

        Synchronizer* sync = m->getSynchronizer();

        // ignore action?  what if we're in a script for block!!?
        Track* track = m->getTrack();

		if (mTrack) {
            Trace(2, "Setting track sync master to %ld", (long)track->getDisplayNumber());
            sync->setTrackSyncMaster(track);
        }
        else if (mMidi) {
            Trace(2, "Setting out sync master to %ld", (long)track->getDisplayNumber());
            sync->setOutSyncMaster(track);
        }
        else {
            Trace(2, "Setting track and out sync master to %ld", (long)track->getDisplayNumber());
            sync->setTrackSyncMaster(track);
            sync->setOutSyncMaster(track);
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
