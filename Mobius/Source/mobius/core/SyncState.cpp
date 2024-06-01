/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Various state related to synchronization maintained on each Track.
 * These could just go on Track, but there are a lot of them and I
 * like making the relationship clearer.
 * 
 * The fields that define how sync will be performed are taken
 * from the Setup and Preset.  Once the first loop in a track is
 * recorded using these sync parameters, the SyncState is "locked"
 * and we will use those parameters until all loops in the track have
 * been reset.  Any changes to the Setup or Preset will be ignored
 * while the SyncState is locked.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "../../util/Trace.h"

#include "Mobius.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Preset.h"
#include "Track.h"
#include "../../model/Setup.h"
#include "Synchronizer.h"

#include "Event.h"
#include "SyncState.h"

/****************************************************************************
 *                                                                          *
 *                                 SYNC STATE                               *
 *                                                                          *
 ****************************************************************************/

SyncState::SyncState(Track* t)
{
    mTrack = t;
    mLocked = false;

    mSyncSource = SYNC_NONE;
    mSyncUnit = SYNC_UNIT_BEAT;
    mSyncTrackUnit = TRACK_UNIT_LOOP;

    initRecordState();

    mPreRealignFrame = 0;
}

SyncState::~SyncState()
{
}

void SyncState::initRecordState()
{
    mRecording = false;
    mRounding = false;
    mTrackerLocked = false;
    mOriginPulse = 0;
    mCyclePulses = 0;
    mRecordPulses = 0;
    mTrackerPulses = 0;
    mTrackerFrames = 0;
    mTrackerBeatsPerBar = 0;
}

//
// Sync Options
//

/**
 * Return the sync source defined for this track in the setup
 */
SyncSource SyncState::getDefinedSyncSource()
{
    if (!mLocked) {
        SetupTrack* st = mTrack->getSetup();
        if (st == NULL)
          mSyncSource = SYNC_DEFAULT;
        else
          mSyncSource = st->getSyncSource();

        if (mSyncSource == SYNC_DEFAULT) {
            Setup* s = mTrack->getMobius()->getSetup();
            if (s == NULL)
              mSyncSource = SYNC_NONE;
            else {
                mSyncSource = s->getSyncSource();

                // not supposed to be DEFAULT but I've seen it
                if (mSyncSource == SYNC_DEFAULT)
                  mSyncSource = SYNC_NONE;
            }
        }
    }

    return mSyncSource;
}

/**
 * This calculates the effective sync source for a track.
 * This is more than just the sync source specified in the setup, it
 * also factors in the state of the other tracks.
 * 
 * SYNC_NONE, SYNC_MIDI, and SYNC_HOST will be returned
 * as they are in the setup.
 *
 * SYNC_OUT and SYNC_TRACK are complicated.  SYNC_OUT is returned
 * only if this track is the out sync master track, otherwise
 * it will be SYNC_NONE.
 *
 * 
 */
SyncSource SyncState::getEffectiveSyncSource()
{
    SyncSource src = getDefinedSyncSource();

    if (src == SYNC_OUT) {
        Synchronizer* sync = mTrack->getSynchronizer();
        Track* master = sync->getOutSyncMaster();
        if (master != NULL && master != mTrack) {
            // we fall back to track sync
            master = sync->getTrackSyncMaster();
            if (master != NULL)
              src = SYNC_TRACK;
            else {
                // This can happen when we've just defined the 
                // out sync master track and we call informFollowers()
                // which calls getEffectiveSyncSource, don't warn
                //Trace(1, "SyncState: Unable to locate track sync master!\n");
                src = SYNC_NONE;
            }
        }
    }
    else if (src == SYNC_TRACK) {
        Synchronizer* sync = mTrack->getSynchronizer();
        Track* master = sync->getTrackSyncMaster();
        if (master == NULL || master == mTrack)
          src = SYNC_NONE;
    }
    
    return src;
}

SyncUnit SyncState::getSyncUnit()
{
    if (!mLocked) {
        Setup* s = mTrack->getMobius()->getSetup();
        if (s != NULL)
          mSyncUnit = s->getSyncUnit();
        else
          mSyncUnit = SYNC_UNIT_BEAT;
    }
    return mSyncUnit;
}

SyncTrackUnit SyncState::getSyncTrackUnit()
{
    if (!mLocked) {
        SetupTrack* st = mTrack->getSetup();

        if (st == NULL)
          mSyncTrackUnit = TRACK_UNIT_DEFAULT;
        else
          mSyncTrackUnit = st->getSyncTrackUnit();

        if (mSyncTrackUnit == TRACK_UNIT_DEFAULT) {
            Setup* s = mTrack->getMobius()->getSetup();
            if (s == NULL)
              mSyncTrackUnit = TRACK_UNIT_LOOP;
            else {
                mSyncTrackUnit = s->getSyncTrackUnit();

                if (mSyncTrackUnit == TRACK_UNIT_DEFAULT)
                  mSyncTrackUnit = TRACK_UNIT_LOOP;
            }
        }
    }
    return mSyncTrackUnit;
}

/**
 * We don't copy this since it isn't necessary until the end.
 */
bool SyncState::isManualStart()
{
    bool isManual = false;
    Setup* s = mTrack->getMobius()->getSetup();
    if (s != NULL)  
      isManual = s->isManualStart();
    return isManual;
}

//
// Record status
//

bool SyncState::isRecording()
{
    return mRecording;
}

bool SyncState::isRounding()
{
    return mRounding;
}

bool SyncState::wasTrackerLocked()
{
    return mTrackerLocked;
}

int SyncState::getOriginPulse()
{
    return mOriginPulse;
}

int SyncState::getCyclePulses()
{
    return mCyclePulses;
}

int SyncState::getRecordPulses()
{
    return mRecordPulses;
}

int SyncState::getTrackerPulses()
{
    return mTrackerPulses;
}

long SyncState::getTrackerFrames()
{
    return mTrackerFrames;
}

int SyncState::getTrackerBeatsPerBar()
{
    return mTrackerBeatsPerBar;
}

/****************************************************************************
 *                                                                          *
 *                                  LOCKING                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Lock the state from fugure config updates.
 * This is actually not called directly, it will be called as a side
 * effect of startRecording().
 */
void SyncState::lock()
{
    // call each of the accessors once to get the state refreshed, then lock
    getDefinedSyncSource();
    getSyncUnit();
    getSyncTrackUnit();

    mLocked = true;
}

void SyncState::unlock()
{
    mLocked = false;
}

/****************************************************************************
 *                                                                          *
 *                            RECORDING LIFECYCLE                           *
 *                                                                          *
 ****************************************************************************/

/**
 * Begin recording in this track passing the number of beats in a cycle.
 * This will lock the sync options if not already locked.
 * Passing the event is optional and done only when we're starting the
 * recording exactly on a pulse boundary.
 */
void SyncState::startRecording(int originPulse,
                                      int cyclePulses, 
                                      int beatsPerBar,
                                      bool trackerLocked)
{
    mRecording = true;
    mRounding = false;
    mTrackerLocked = trackerLocked;
    mOriginPulse = originPulse;
    mCyclePulses = cyclePulses;
    mRecordPulses = 0;
    mTrackerBeatsPerBar = beatsPerBar;

    // once recording starts we need stable parameters
    // actually should have locked earlier because Synchronizer has been making
    // assumptions based on the SyncSource
    lock();
}

void SyncState::pulse()
{
    mRecordPulses++;
}

/**
 * An awful kludge for SYNC_MIDI.
 * Before the tracker is locked we'll get a pulse per clock.
 * After the tracker is locked we only get beat/bar pulses.
 * The problem is that the pulse count here has to be consistently clocks,
 * so Synchronizer will call this with an adjustment (23) if necessary.
 */
void SyncState::addPulses(int extra)
{
    mRecordPulses += extra;
}

void SyncState::scheduleStop(int pulses, long frames)
{
    mRounding = true;
    mTrackerPulses = pulses;
    mTrackerFrames = frames;
}

void SyncState::stopRecording()
{
    initRecordState();
    // note that this does not unlock, unlock happens
    // only when the loop is reset
}

/**
 * Set the boundary event received during an interrupt.
 * This will be set to null at the beginning of each interrupt
 * and then set to the event type of any of the generated boundary
 * events encountered during the interrupt: SubcycleEvent, CycleEvent, LoopEvent.
 */
void SyncState::setBoundaryEvent(EventType* type)
{
    mBoundaryEvent = type;
}

EventType* SyncState::getBoundaryEvent()
{
    return mBoundaryEvent;
}

/****************************************************************************
 *                                                                          *
 *                                 UNIT TESTS                               *
 *                                                                          *
 ****************************************************************************/

void SyncState::setPreRealignFrame(long frame)
{
    mPreRealignFrame = frame;
}

long SyncState::getPreRealignFrame()
{
    return mPreRealignFrame;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
