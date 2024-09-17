/*
 * Model for a "track setup", a collection of parameters that apply to 
 * all tracks.
 *
 * This is still used in engine code so avoid changes until we can
 * finish porting that.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../util/Util.h"
#include "../util/MidiUtil.h"

// uses StringList for resettables
#include "../util/List.h"

#include "UIParameter.h"

#include "ExValue.h"
#include "Preset.h"
#include "UserVariable.h"

#include "Structure.h"
#include "Setup.h"

/****************************************************************************
 *                                                                          *
 *                                 CONSTANTS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Parameter defaults.
 * Note that the unit tests depend on some of these, do NOT change them
 * without understanding the consequences for the tests.
 */
#define DEFAULT_MIN_TEMPO 20
#define DEFAULT_MAX_TEMPO 300
#define DEFAULT_BAR_BEATS 4

/****************************************************************************
 *                                                                          *
 *                               XML CONSTANTS                              *
 *                                                                          *
 ****************************************************************************/

#define EL_SETUP_TRACK "SetupTrack"
#define EL_VARIABLES "Variables"

#define ATT_BINDINGS "bindings"
#define ATT_MIDI_CONFIG "midiConfig"

#define ATT_NAME "name"
#define ATT_ACTIVE "active"
#define ATT_TRACK_GROUPS "trackGroups"
#define ATT_RESETABLES "reset"
#define ATT_ACTIVE "active"

/****************************************************************************
 *                                                                          *
 *                                  GLOBALS                                 *
 *                                                                          *
 ****************************************************************************/

// Global functions to return trace names for Setup enumerations
// !! figure out a way to share these with Parameter definitions

const char* GetSyncSourceName(SyncSource src) 
{
    const char* name = "???";
    switch (src) {
        case SYNC_DEFAULT: name = "Default"; break;
        case SYNC_NONE: name = "None"; break;
        case SYNC_TRACK: name = "Track"; break;
        case SYNC_OUT: name = "Out"; break;
        case SYNC_HOST: name = "Host"; break;
        case SYNC_MIDI: name = "MIDI"; break;
    }
    return name;
}

/****************************************************************************
 *                                                                          *
 *   								SETUP                                   *
 *                                                                          *
 ****************************************************************************/

Setup::Setup()
{
	mTracks = nullptr;
	mActiveTrack = 0;
    mDefaultPresetName = nullptr;
	mResetRetains = nullptr;
	mBindings = nullptr;

    initParameters();
}

Setup::~Setup()
{
	delete mTracks;
	delete mBindings;
    delete mResetRetains;
    delete mDefaultPresetName;
}

Structure* Setup::clone()
{
    return new Setup(this);
}

/**
 * Restore the default parameters expected by the unit tests.
 */
void Setup::initParameters()
{
    // Sync
    mSyncSource         = SYNC_TRACK;
    mSyncUnit           = SYNC_UNIT_BEAT;
    mSyncTrackUnit      = TRACK_UNIT_LOOP;
    mManualStart        = false;
	mMinTempo			= DEFAULT_MIN_TEMPO;
	mMaxTempo			= DEFAULT_MAX_TEMPO;
	mBeatsPerBar		= DEFAULT_BAR_BEATS;
	mMuteSyncMode   	= MUTE_SYNC_TRANSPORT;
    mResizeSyncAdjust   = SYNC_ADJUST_NONE;
	mSpeedSyncAdjust	= SYNC_ADJUST_NONE;
	mRealignTime		= REALIGN_START;
	mOutRealignMode		= REALIGN_RESTART;
}

Setup::Setup(Setup* src)
{
	mTracks = nullptr;
	mBindings = nullptr;
   	mResetRetains = nullptr;
    mDefaultPresetName = nullptr;

    setName(src->getName());
    setDefaultPresetName(src->getDefaultPresetName());
    setResetRetains(src->getResetRetains());

    mActiveTrack = src->getActiveTrack();
    setBindings(src->getBindings());
    mSyncSource = src->getSyncSource();
    mSyncUnit = src->getSyncUnit();
    mSyncTrackUnit = src->getSyncTrackUnit();
    mManualStart = src->isManualStart();
    mMinTempo = src->getMinTempo();
    mMaxTempo = src->getMaxTempo();
    mBeatsPerBar = src->getBeatsPerBar();
    mMuteSyncMode = src->getMuteSyncMode();
    mResizeSyncAdjust = src->getResizeSyncAdjust();
    mSpeedSyncAdjust = src->getSpeedSyncAdjust();
    mRealignTime = src->getRealignTime();
    mOutRealignMode = src->getOutRealignMode();


    SetupTrack* last = nullptr;
    SetupTrack* srcTrack = src->getTracks();
    while (srcTrack != nullptr) {
        SetupTrack* copy = new SetupTrack(srcTrack);
        if (last == nullptr)
          mTracks = copy;
        else
          last->setNext(copy);
        last = copy;
        srcTrack = srcTrack->getNext();
    }
}

/**
 * Put the setup into the standard state for unit tests.
 */
void Setup::reset(Preset* p)
{
	mActiveTrack = 0;

	setDefaultPresetName(nullptr);
    
    // need a default list of these?
    setResetRetains(nullptr);

    // don't really care what the binding configs are
	setBindings(nullptr);

    // start over with a new SetupTrack list
    setTracks(nullptr);

    // formerly copied the given preset name into
    // every track because we didn't have the notion
    // of the defaultPreset, shouldn't need that any more
    for (int i = 0 ; i < DEFAULT_TRACK_COUNT ; i++) {
        SetupTrack* t = getTrack(i);
        t->reset();
        if (p != nullptr)
          t->setTrackPresetName(p->getName());
    }

    initParameters();
}

SetupTrack* Setup::getTracks()
{
	return mTracks;
}

SetupTrack* Setup::stealTracks()
{
	SetupTrack* list = mTracks;
	mTracks = nullptr;
	return list;
}

void Setup::setTracks(SetupTrack* list)
{
    if (list != mTracks) {
		delete mTracks;
		mTracks = list;
    }
}

SetupTrack* Setup::getTrack(int index)
{
	SetupTrack* track = mTracks;
	SetupTrack* prev = nullptr;

	for (int i = 0 ; i <= index ; i++) {
		if (track == nullptr) {
			track = new SetupTrack();
			if (prev == nullptr)
			  mTracks = track;
			else
			  prev->setNext(track);
		}
		if (i < index) {
			prev = track;
			track = track->getNext();
		}
	}
	return track;
}

/****************************************************************************
 *                                                                          *
 *                              SETUP PARAMETERS                            *
 *                                                                          *
 ****************************************************************************/

void Setup::setDefaultPresetName(const char* name)
{
	delete mDefaultPresetName;
	mDefaultPresetName = CopyString(name);
}

const char* Setup::getDefaultPresetName()
{
	return mDefaultPresetName;
}

void Setup::setBindings(const char* name)
{
	delete mBindings;
	mBindings = CopyString(name);
}

const char* Setup::getBindings()
{
	return mBindings;
}

int Setup::getActiveTrack()
{
	return mActiveTrack;
}

void Setup::setActiveTrack(int i)
{
	mActiveTrack = i;
}

void Setup::setResetRetains(const char* csv)
{
    delete mResetRetains;
    mResetRetains = CopyString(csv);
}

const char* Setup::getResetRetains()
{
	return mResetRetains;
}

/**
 * This used to be a StringList, now it's just a CSV.
 * Speed of comparision is about the same doing a substring match
 * vs. a StringList iteration, however it does mean that the name
 * of any one parameter can't be a substring of another.  This is the
 * case now, but it's fragile.  The only edge case is "feedback" and
 * "altFeedback" which won't conflict as long as we're doing case insensntive
 * comparison.
 *
 * Update: not used any more, this is now a flag on ParameterProperties
 */
#if 0
bool Setup::isResetRetain(const char* parameterName)
{
	return (mResetRetains != nullptr &&
            IndexOf(mResetRetains, parameterName) >= 0);
}
#endif

SyncSource Setup::getSyncSource()
{
    return mSyncSource;
}

void Setup::setSyncSource(SyncSource src)
{
    mSyncSource = src;
}

SyncUnit Setup::getSyncUnit()
{
    return mSyncUnit;
}

void Setup::setSyncUnit(SyncUnit src)
{
    mSyncUnit = src;
}

SyncTrackUnit Setup::getSyncTrackUnit()
{
    return mSyncTrackUnit;
}

void Setup::setSyncTrackUnit(SyncTrackUnit unit)
{
    mSyncTrackUnit = unit;
}

bool Setup::isManualStart()
{
    return mManualStart;
}

void Setup::setManualStart(bool b)
{
    mManualStart = b;
}

int Setup::getMinTempo()
{
	return mMinTempo;
}

void Setup::setMinTempo(int i)
{
    if (i == 0) i = DEFAULT_MIN_TEMPO;
	mMinTempo = i;
}

int Setup::getMaxTempo()
{
	return mMaxTempo;
}

void Setup::setMaxTempo(int i)
{
    if (i == 0) i = DEFAULT_MAX_TEMPO;
	mMaxTempo = i;
}

int Setup::getBeatsPerBar()
{
	return mBeatsPerBar;
}

void Setup::setBeatsPerBar(int i)
{
	mBeatsPerBar = i;
}

void Setup::setMuteSyncMode(MuteSyncMode i) {
	mMuteSyncMode = i;
}

void Setup::setMuteSyncMode(int i) {
	setMuteSyncMode((MuteSyncMode)i);
}

MuteSyncMode Setup::getMuteSyncMode() {
	return mMuteSyncMode;
}

void Setup::setResizeSyncAdjust(SyncAdjust i) {
	mResizeSyncAdjust = i;
}

void Setup::setResizeSyncAdjust(int i) {
	setResizeSyncAdjust((SyncAdjust)i);
}

SyncAdjust Setup::getResizeSyncAdjust() {
	return mResizeSyncAdjust;
}

void Setup::setSpeedSyncAdjust(SyncAdjust i) {
	mSpeedSyncAdjust = i;
}

void Setup::setSpeedSyncAdjust(int i) {
	setSpeedSyncAdjust((SyncAdjust)i);
}

SyncAdjust Setup::getSpeedSyncAdjust() {
	return mSpeedSyncAdjust;
}

void Setup::setRealignTime(RealignTime t) {
	mRealignTime = t;
}

void Setup::setRealignTime(int i) {
	setRealignTime((RealignTime)i);
}

RealignTime Setup::getRealignTime() {
	return mRealignTime;
}

void Setup::setOutRealignMode(OutRealignMode m) {
	mOutRealignMode = m;
}

void Setup::setOutRealignMode(int i) {
	setOutRealignMode((OutRealignMode)i);
}

OutRealignMode Setup::getOutRealignMode() {
	return mOutRealignMode;
}

/****************************************************************************
 *                                                                          *
 *                                SETUP TRACK                               *
 *                                                                          *
 ****************************************************************************/

SetupTrack::SetupTrack()
{
	mNext = nullptr;
    mName = nullptr;
	mTrackPresetName = nullptr;
	mVariables = nullptr;
	reset();
}

SetupTrack::~SetupTrack()
{
	SetupTrack *el, *next;

	delete mName;
	delete mTrackPresetName;
	delete mVariables;

	for (el = mNext ; el != nullptr ; el = next) {
		next = el->getNext();
		el->setNext(nullptr);
		delete el;
	}
}

SetupTrack::SetupTrack(SetupTrack* src)
{   
	mNext = nullptr;
    mName = nullptr;
	mTrackPresetName = nullptr;
	mVariables = nullptr;
    
    setName(src->getName());
    setTrackPresetName(src->getTrackPresetName());

	mFocusLock = src->isFocusLock();
    // shouldn't need to be dealing with group numbers any more
	mGroup = src->getGroupNumberDeprecated();
    mGroupName = src->getGroupName();
	mInputLevel = src->getInputLevel();
	mOutputLevel = src->getOutputLevel();
	mFeedback = src->getFeedback();
	mAltFeedback = src->getAltFeedback();
	mPan = src->getPan();
	mMono = src->isMono();
	mAudioInputPort = src->getAudioInputPort();
	mAudioOutputPort = src->getAudioOutputPort();
	mPluginInputPort = src->getPluginInputPort();
	mPluginOutputPort = src->getPluginOutputPort();
    mSyncSource = src->getSyncSource();
    mSyncTrackUnit = src->getSyncTrackUnit();

	// !! TODO: copy mVariables
}

/**
 * Called by the UI to return the track to an initial state.
 * Since we've already been initialized have to be careful
 * about the preset name.
 * !! not sure about variables yet
 * This is also used by the UnitTestSetup script command when
 * initializing the default test setup.
 */
void SetupTrack::reset()
{
	setTrackPresetName(nullptr);
    setName(nullptr);
	mFocusLock = false;
    mGroup = 0;
    mGroupName = "";
	mInputLevel = 127;
	mOutputLevel = 127;
	mFeedback = 127;
	mAltFeedback = 127;
	mPan = 64;
	mMono = false;
	mAudioInputPort = 0;
	mAudioOutputPort = 0;
	mPluginInputPort = 0;
	mPluginOutputPort = 0;
    mSyncSource = SYNC_DEFAULT;
    mSyncTrackUnit = TRACK_UNIT_DEFAULT;
}

/**
 * Capture the state of an active Track.
 * Don't like having this here, move out to where
 * all the state export stuff can live
 */
#if 0
void SetupTrack::capture(MobiusState* state)
{
    TrackState* t = state->track;

    // if we continue to do this, should avoid setting
    // this if the runtime preset is the same as the
    // default preset
	setTrackPresetName(t->preset->getName());

	mFocusLock = t->focusLock;
	mGroup = t->group;
    mGroupName = ???
	mInputLevel = t->inputLevel;
	mOutputLevel = t->outputLevel;
	mFeedback = t->feedback;
	mAltFeedback = t->altFeedback;
	mPan = t->pan;

    // not there yet...
	//mMono = t->mono;

	// !! track only has one set of ports for both vst/audio
	// does it even make sense to capture these?
    // Since MobiusState doesn't have them, punt...
    /*
	mAudioInputPort = t->getInputPort();
	mAudioOutputPort = t->getOutputPort();
	mPluginInputPort = t->getInputPort();
	mPluginOutputPort = t->getOutputPort();
    */

    // can no longer get to the Track's Setup
    // via MobiusState
    /*
    SetupTrack* st = t->getSetup();
    mSyncSource = st->getSyncSource();
    mSyncTrackUnit = st->getSyncTrackUnit();
    */

}
#endif



void SetupTrack::setNext(SetupTrack* s) 
{
	mNext = s;
}

SetupTrack* SetupTrack::getNext()
{
	return mNext;
}

void SetupTrack::setName(const char* s)
{
	delete mName;
	mName = CopyString(s);
}

const char* SetupTrack::getName()
{
	return mName;
}

void SetupTrack::setTrackPresetName(const char* name)
{
	delete mTrackPresetName;
	mTrackPresetName = CopyString(name);
}

const char* SetupTrack::getTrackPresetName()
{
	return mTrackPresetName;
}

void SetupTrack::setFocusLock(bool b)
{
	mFocusLock = b;
}

bool SetupTrack::isFocusLock()
{
	return mFocusLock;
}

int SetupTrack::getGroupNumberDeprecated()
{
    return mGroup;
}

void SetupTrack::setGroupNumberDeprecated(int i)
{
    mGroup = i;
}

juce::String SetupTrack::getGroupName()
{
    return mGroupName;
}

void SetupTrack::setGroupName(juce::String s)
{
    mGroupName = s;
}

void SetupTrack::setInputLevel(int i)
{
	mInputLevel = i;
}

int SetupTrack::getInputLevel()
{
	return mInputLevel;
}

void SetupTrack::setOutputLevel(int i)
{
	mOutputLevel = i;
}

int SetupTrack::getOutputLevel()
{
	return mOutputLevel;
}

void SetupTrack::setFeedback(int i)
{
	mFeedback = i;
}

int SetupTrack::getFeedback()
{
	return mFeedback;
}

void SetupTrack::setAltFeedback(int i)
{
	mAltFeedback = i;
}

int SetupTrack::getAltFeedback()
{
	return mAltFeedback;
}

void SetupTrack::setPan(int i)
{
	mPan = i;
}

int SetupTrack::getPan()
{
	return mPan;
}

void SetupTrack::setMono(bool b)
{
	mMono = b;
}

bool SetupTrack::isMono()
{
	return mMono;
}

void SetupTrack::setAudioInputPort(int i)
{
	mAudioInputPort = i;
}

int SetupTrack::getAudioInputPort()
{
	return mAudioInputPort;
}

void SetupTrack::setAudioOutputPort(int i)
{
	mAudioOutputPort = i;
}

int SetupTrack::getAudioOutputPort()
{
	return mAudioOutputPort;
}

void SetupTrack::setPluginInputPort(int i)
{
	mPluginInputPort = i;
}

int SetupTrack::getPluginInputPort()
{
	return mPluginInputPort;
}

void SetupTrack::setPluginOutputPort(int i)
{
	mPluginOutputPort = i;
}

int SetupTrack::getPluginOutputPort()
{
	return mPluginOutputPort;
}

SyncSource SetupTrack::getSyncSource()
{
    return mSyncSource;
}

void SetupTrack::setSyncSource(SyncSource src)
{
    mSyncSource = src;
}

SyncTrackUnit SetupTrack::getSyncTrackUnit()
{
    return mSyncTrackUnit;
}

void SetupTrack::setSyncTrackUnit(SyncTrackUnit unit)
{
    mSyncTrackUnit = unit;
}

UserVariables* SetupTrack::getVariables()
{
    return mVariables;
}

void SetupTrack::setVariables(UserVariables* vars)
{
    delete mVariables;
    mVariables = vars;
}

void SetupTrack::setVariable(const char* name, ExValue* value)
{
	if (name != nullptr) {
		if (mVariables == nullptr)
		  mVariables = new UserVariables();
		mVariables->set(name, value);
	}
}

void SetupTrack::getVariable(const char* name, ExValue* value)
{
	value->setNull();
	if (mVariables != nullptr)
	  mVariables->get(name, value);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
