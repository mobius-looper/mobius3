/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for the Mobius core configuration.
 * UIConfig has a model for most of the UI configuration.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../util/Trace.h"
#include "../util/Util.h"
#include "../util/List.h"
#include "../util/MidiUtil.h"

#include "Binding.h"
#include "Preset.h"
#include "Setup.h"
#include "ScriptConfig.h"
#include "SampleConfig.h"
#include "XmlRenderer.h"
#include "ValueSet.h"

#include "MobiusConfig.h"

// defined down in Audio.h, think about where this should live

/**
 * Maximum number of frames that may be used for cross fading.
 */
#define AUDIO_MAX_FADE_FRAMES 256

/**
 * Minimum number of frames that may be used for cross fading.
 * Formerly we allowed zero here, but it's easy for bad config files
 * to leave this out or set it to zero since zero usually means "default".
 * 
 */
#define AUDIO_MIN_FADE_FRAMES 16

/**
 * Default number of frames to use during fade in/out
 * of a newly recorded segment.  At a sample rate of 44100, 
 * a value of 441 results in a 1/10 second fade.  But
 * we don't really need that much.    If the range is too large
 * you hear "breathing".
 *
 * UDPATE: Since we use static buffers for this and the max is only 256, it
 * isn't really possible to set large values for effect.  While fade frames
 * can be set in a global parameter this is no longer exposed in the user interface.
 */
#define AUDIO_DEFAULT_FADE_FRAMES 128

/****************************************************************************
 *                                                                          *
 *   								CONFIG                                  *
 *                                                                          *
 ****************************************************************************/

MobiusConfig::MobiusConfig()
{
	init();
}

// what the hell does this do?
MobiusConfig::MobiusConfig(bool dflt)
{
	init();
    mDefault = dflt;
}

void MobiusConfig::init()
{
    mError[0] = 0;
    mDefault = false;
    mHistory = nullptr;
	mQuickSave = nullptr;

	mNoiseFloor = DEFAULT_NOISE_FLOOR;
	mInputLatency = 0;
	mOutputLatency = 0;
	mFadeFrames = AUDIO_DEFAULT_FADE_FRAMES;
	mMaxSyncDrift = DEFAULT_MAX_SYNC_DRIFT;
	mCoreTracks = DEFAULT_TRACKS;
    mTrackGroups = DEFAULT_TRACK_GROUPS;
    mMaxLoops = DEFAULT_MAX_LOOPS;
    mLongPress = DEFAULT_LONG_PRESS_MSECS;

	mFocusLockFunctions = nullptr;
	mMuteCancelFunctions = nullptr;
	mConfirmationFunctions = nullptr;
	mAltFeedbackDisables = nullptr;

	mPresets = nullptr;
    
	mSetups = nullptr;
    mStartingSetupName = nullptr;
    mStartingSetup = nullptr;
    
	mBindingSets = nullptr;
    
    mScriptConfig = nullptr;
    mSampleConfig = nullptr;

//     mSampleRate = SAMPLE_RATE_44100;
	mMonitorAudio = false;
    mHostRewinds = false;
    mAutoFeedbackReduction = false;
    mIsolateOverdubs = false;
    mIntegerWaveFile = false;
	mSpreadRange = DEFAULT_SPREAD_RANGE;
	mTracePrintLevel = 1;
	mTraceDebugLevel = 2;
	mSaveLayers = false;
	mDriftCheckPoint = DRIFT_CHECK_LOOP;
    mGroupFocusLock = false;

    mNoPresetChanges = false;
    mNoSetupChanges = false;

    mNoSyncBeatRounding = false;

    mEdpisms = false;
}

MobiusConfig::~MobiusConfig()
{
    // delete the history list if we have one
	MobiusConfig *el, *next;
	for (el = mHistory ; el != nullptr ; el = next) {
		next = el->getHistory();
		el->setHistory(nullptr);
		delete el;
	}

	delete mQuickSave;

	delete mFocusLockFunctions;
	delete mMuteCancelFunctions;
	delete mConfirmationFunctions;
	delete mAltFeedbackDisables;
	delete mPresets;
    delete mSetups;
    delete mStartingSetupName;
    delete mBindingSets;
    delete mScriptConfig;
	delete mSampleConfig;
}

/**
 * This doesn't have a proper copy constructor so we have
 * to use XML.
 */
MobiusConfig* MobiusConfig::clone()
{
    XmlRenderer xr;
    MobiusConfig* neu = xr.clone(this);

    // these are not in the XML rendering and need
    // to follow the clone through the layers from the UI
    // down to the core
    neu->setupsEdited = setupsEdited;
    neu->presetsEdited = presetsEdited;

    return neu;
}

bool MobiusConfig::isDefault()
{
    return mDefault;
}

// GroupDefinitions aren't structdures which is kind of a pain
int MobiusConfig::getGroupOrdinal(juce::String name)
{
    int ordinal = -1;
    for (int i = 0 ; i < groups.size() ; i++) {
        GroupDefinition* def = groups[i];
        if (def->name == name) {
            ordinal = i;
            break;
        }
    }
    return ordinal;
}

void MobiusConfig::setHistory(MobiusConfig* config)
{
    mHistory = config;
}

MobiusConfig* MobiusConfig::getHistory()
{
    return mHistory;
}

int MobiusConfig::getHistoryCount()
{
    int count = 0;
    for (MobiusConfig* c = this ; c != nullptr ; c = c->getHistory())
      count++;
    return count;
}

void MobiusConfig::setMonitorAudio(bool b)
{
	mMonitorAudio = b;
}

bool MobiusConfig::isMonitorAudio()
{
	return mMonitorAudio;
}

void MobiusConfig::setHostRewinds(bool b)
{
	mHostRewinds = b;
}

bool MobiusConfig::isHostRewinds()
{
	return mHostRewinds;
}

void MobiusConfig::setAutoFeedbackReduction(bool b)
{
	mAutoFeedbackReduction = b;
}

bool MobiusConfig::isAutoFeedbackReduction()
{
	return mAutoFeedbackReduction;
}

void MobiusConfig::setIsolateOverdubs(bool b)
{
	mIsolateOverdubs = b;
}

bool MobiusConfig::isIsolateOverdubs()
{
	return mIsolateOverdubs;
}

void MobiusConfig::setIntegerWaveFile(bool b)
{
	mIntegerWaveFile = b;
}

bool MobiusConfig::isIntegerWaveFile()
{
	return mIntegerWaveFile;
}

void MobiusConfig::setSpreadRange(int i)
{
	// backward compatibility with old files
	if (i <= 0) 
      i = DEFAULT_SPREAD_RANGE;
    else if (i > MAX_RATE_STEP)
      i = MAX_RATE_STEP;

	mSpreadRange = i;
}

int MobiusConfig::getSpreadRange()
{
	return mSpreadRange;
}

void MobiusConfig::setTracePrintLevel(int i) {
	mTracePrintLevel = i;
}

int MobiusConfig::getTracePrintLevel() {
	return mTracePrintLevel;
}

void MobiusConfig::setTraceDebugLevel(int i) {
	mTraceDebugLevel = i;
}

int MobiusConfig::getTraceDebugLevel() {
	return mTraceDebugLevel;
}

void MobiusConfig::setSaveLayers(bool b) {
	mSaveLayers = b;
}

bool MobiusConfig::isSaveLayers() {
	return mSaveLayers;
}

int MobiusConfig::getNoiseFloor()
{
	return mNoiseFloor;
}

void MobiusConfig::setNoiseFloor(int i)
{
    // this has been stuck zero for quite awhile, initialize it
    if (i == 0) i = DEFAULT_NOISE_FLOOR;
	mNoiseFloor = i;
}

int MobiusConfig::getCoreTracks()
{
	return mCoreTracks;
}

void MobiusConfig::setCoreTracks(int i)
{
	if (i == 0) i = DEFAULT_TRACKS;
	mCoreTracks = i;
}

int MobiusConfig::getTrackGroupsDeprecated()
{
	return mTrackGroups;
}

void MobiusConfig::setTrackGroupsDeprecated(int i)
{
	mTrackGroups = i;
}

int MobiusConfig::getMaxLoops()
{
	return mMaxLoops;
}

void MobiusConfig::setMaxLoops(int i)
{
	mMaxLoops = i;
}

int MobiusConfig::getInputLatency()
{
	return mInputLatency;
}

void MobiusConfig::setInputLatency(int i)
{
	mInputLatency = i;
}

int MobiusConfig::getOutputLatency()
{
	return mOutputLatency;
}

void MobiusConfig::setOutputLatency(int i)
{
	mOutputLatency = i;
}

/**
 * Hmm, wanted to let 0 default because upgrades won't have
 * this parameter set.  But this leaves no way to turn off long presses.
 */
void MobiusConfig::setLongPress(int i)
{
	if (i <= 0) 
	  i = DEFAULT_LONG_PRESS_MSECS;
	mLongPress = i;
}

int MobiusConfig::getLongPress()
{
	return mLongPress;
}

/**
 * Originally this was a configurable parameter but the
 * range had to be severly restricted to prevent stack
 * overflow since fade buffers are allocated on the stack.
 * With the reduced range there isn't much need to set this
 * so force it to 128.
 */
int MobiusConfig::getFadeFrames()
{
	return mFadeFrames;
}

void MobiusConfig::setFadeFrames(int i)
{
    // force this to a normal value
	if (i <= 0)
      i = AUDIO_DEFAULT_FADE_FRAMES;

    else if (i < AUDIO_MIN_FADE_FRAMES)
      i = AUDIO_MIN_FADE_FRAMES;

    else if (i > AUDIO_MAX_FADE_FRAMES)
      i = AUDIO_MAX_FADE_FRAMES;

    mFadeFrames = i;
}

int MobiusConfig::getMaxSyncDrift()
{
	return mMaxSyncDrift;
}

void MobiusConfig::setMaxSyncDrift(int i)
{
    // this was stuck low for many people, try to correct that
    if (i == 0) i = 512;
    mMaxSyncDrift = i;
}

void MobiusConfig::setDriftCheckPoint(DriftCheckPoint dcp)
{
	mDriftCheckPoint = dcp;
}

DriftCheckPoint MobiusConfig::getDriftCheckPoint()
{
	return mDriftCheckPoint;
}

ScriptConfig* MobiusConfig::getScriptConfigObsolete()
{
    return mScriptConfig;
}

void MobiusConfig::setScriptConfigObsolete(ScriptConfig* dc)
{
    if (dc != mScriptConfig) {
        delete mScriptConfig;
        mScriptConfig = dc;
    }
}

void MobiusConfig::setQuickSave(const char* s) 
{
	delete mQuickSave;
	mQuickSave = CopyString(s);
}

const char* MobiusConfig::getQuickSave()
{
	return mQuickSave;
}

void MobiusConfig::setSampleConfig(SampleConfig* s)
{
	if (mSampleConfig != s) {
		delete mSampleConfig;
		mSampleConfig = s;
	}
}

SampleConfig* MobiusConfig::getSampleConfig()
{
	return mSampleConfig;
}

StringList* MobiusConfig::getFocusLockFunctions()
{
	return mFocusLockFunctions;
}

void MobiusConfig::setFocusLockFunctions(StringList* l) 
{
	delete mFocusLockFunctions;
	mFocusLockFunctions = l;
}

StringList* MobiusConfig::getMuteCancelFunctions()
{
	return mMuteCancelFunctions;
}

void MobiusConfig::setMuteCancelFunctions(StringList* l) 
{
	delete mMuteCancelFunctions;
	mMuteCancelFunctions = l;
}

StringList* MobiusConfig::getConfirmationFunctions()
{
	return mConfirmationFunctions;
}

void MobiusConfig::setConfirmationFunctions(StringList* l) 
{
	delete mConfirmationFunctions;
	mConfirmationFunctions = l;
}

StringList* MobiusConfig::getAltFeedbackDisables() 
{
	return mAltFeedbackDisables;
}

void MobiusConfig::setAltFeedbackDisables(StringList* l) 
{
	delete mAltFeedbackDisables;
	mAltFeedbackDisables = l;
}

void MobiusConfig::setGroupFocusLock(bool b) {
	mGroupFocusLock = b;
}

bool MobiusConfig::isGroupFocusLock() {
	return mGroupFocusLock;
}

void MobiusConfig::setNoPresetChanges(bool b) {
	mNoPresetChanges = b;
}

bool MobiusConfig::isNoPresetChanges() {
	return mNoPresetChanges;
}

void MobiusConfig::setNoSetupChanges(bool b) {
	mNoSetupChanges = b;
}

bool MobiusConfig::isNoSetupChanges() {
	return mNoSetupChanges;
}

void MobiusConfig::setNoSyncBeatRounding(bool b) {
	mNoSyncBeatRounding = b;
}

bool MobiusConfig::isNoSyncBeatRounding() {
	return mNoSyncBeatRounding;
}

void MobiusConfig::setEdpisms(bool b) {
	mEdpisms = b;
}

bool MobiusConfig::isEdpisms() {
	return mEdpisms;
}

/****************************************************************************
 *                                                                          *
 *                             PRESET MANAGEMENT                            *
 *                                                                          *
 ****************************************************************************/

// The default preset has the same cache issues as startingSetup
// if the UI does list surgery without going through one of these
// methods, the cache can be invalid and point to a deleted object.
// Dislike...

Preset* MobiusConfig:: getPresets() 
{
	return mPresets;
}

void MobiusConfig::setPresets(Preset* list)
{
    if (list != mPresets) {
		delete mPresets;
		mPresets = list;
    }
}
	
void MobiusConfig::addPreset(Preset* p) 
{
    mPresets = (Preset*)Structure::append(mPresets, p);
}

/**
 * Look up a preset by name.
 */
Preset* MobiusConfig::getPreset(const char* name)
{
    return (Preset*)(Structure::find(mPresets, name));
}

/**
 * Look up a preset by ordinal.
 */
Preset* MobiusConfig::getPreset(int ordinal)
{
    return (Preset*)Structure::get(mPresets, ordinal);
}

/**
 * Return the Preset object that is considered the default preset.
 * Formerly had mDefaultPresetName here to guide the search
 * but that was never used.  The "default preset" concept must be
 * implemented in the Setup.
 *
 * What this does not is return the first Preset on the list and
 * bootstraps one if this is a fresh or damanaged configuration
 * that had no presets.  Code expects there to be a least one
 * preset.
 */
Preset* MobiusConfig::getDefaultPreset()
{
    if (mPresets == nullptr) {
        // misconfiguration, bootstrap one
        // note that this does memory allocation and should not
        // be done in the kernel but it's an unusual situation
        Trace(1, "MobiusConfig: Bootstrapping default preset, shouldn't be here\n");
        mPresets = new Preset();
        mPresets->setName("Default");
    }
    return mPresets;
}

/****************************************************************************
 *                                                                          *
 *   						   SETUP MANAGEMENT                             *
 *                                                                          *
 ****************************************************************************/

// StartingSetup cache notes
// This is not reliable.  The config editor will usually replace the Setup
// list but if it just does list surgery we won't know that and won't
// invalidate the cache.  Currently it sets the entire list but
// this is too fragile.

Setup* MobiusConfig::getSetups() 
{
	return mSetups;
}

void MobiusConfig::setSetups(Setup* list)
{
    if (list != mSetups) {
		delete mSetups;
		mSetups = list;
        // setting the list might invalidate the activeSetup name
        mStartingSetup = nullptr;
    }
}
	
void MobiusConfig::addSetup(Setup* s) 
{
    mSetups = (Setup*)Structure::append(mSetups, s);
    // shouldn't invalidate but be safe
    mStartingSetup = nullptr;
}

Setup* MobiusConfig::getSetup(const char* name)
{
    return (Setup*)Structure::find(mSetups, name);
}

Setup* MobiusConfig::getSetup(int ordinal)
{
    return (Setup*)Structure::get(mSetups, ordinal);
}

//
// Starting Setup
//

const char* MobiusConfig::getStartingSetupName()
{
    return mStartingSetupName;
}

void MobiusConfig::setStartingSetupName(const char* name)
{
    delete mStartingSetupName;
    mStartingSetupName = CopyString(name);
    // cache is now invalid
    mStartingSetup = nullptr;
}

/**
 * Return the Setup object that is considered the starting setup.
 * This is a transient runtime value that is calculated by
 * searching the Setup list using the persistent mStartingSetupName.
 * It is cached to avoid a linear string search very time.
 *
 * So system code can depend on the return value being non-null
 * we will boostrap an object if one cannot be found and fix misconfiguration.
 * Not sure I like this but it would be unusual, and avoids crashes.
 * The code here looks complex but it's mostly just handling corner
 * cases that shouldn't happen.
 */
Setup* MobiusConfig::getStartingSetup()
{
    if (mStartingSetup == nullptr) {
        if (mStartingSetupName == nullptr) {
            // misconfiguration, pick the first one
            // note that this does memory allocation and should not
            // be done in the kernel but it's an unusual situation
            // still should switch to static arrays for names
            Trace(1, "Starting setup name not set, default to the first one\n"); 
            if (mSetups == nullptr) {
                // really raw config, bootstrap one
                Trace(1, "Bootstrapping Setup, shouldn't be here\n");
                mSetups = new Setup();
                mSetups->setName("Default");
            }
            setStartingSetupName(mSetups->getName());
        }

        // now the usual lookup by name
        mStartingSetup = getSetup(mStartingSetupName);
        
        if (mStartingSetup == nullptr) {
            // name was misconfigured, should not happen
            Trace(1, "Misconfigured starting setup, %s does not exist, defaulting to first\n",
                  mStartingSetupName);
            // note that setting the name clears the cache so have to do that first
            setStartingSetupName(mSetups->getName());
            mStartingSetup = mSetups;
        }
    }
    return mStartingSetup;
}

/****************************************************************************
 *                                                                          *
 *   						 BINDINGS MANAGEMENT                            *
 *                                                                          *
 ****************************************************************************/
/*
 * The first object on the list is always considered to be the "global"
 * configuration and is always active.  One additional set may be selected
 * that will be merged with the global bindings.  Any number of "overlays"
 * may be added on top of that.
 *
 * This is a weird one because the concept of bindings is purely a UI level
 * thing now. But we have historically managed those inside MobiusConfig and there
 * is a core parameter named "bindings" that old scripts expect to use to change
 * overlay bindings.
 *
 * This really should be made a purely UI thing in uiconfig.xml or broken out
 * to bindings.xml
 * 
 */

BindingSet* MobiusConfig::getBindingSets()
{
	return mBindingSets;
}

void MobiusConfig::setBindingSets(BindingSet* list)
{
    if (list != mBindingSets) {
		delete mBindingSets;
		mBindingSets = list;
    }
}

// no set function, I guess to enforce that you can't take the
// first one away, not really necessary

void MobiusConfig::addBindingSet(BindingSet* bs) 
{
    mBindingSets = (BindingSet*)Structure::append(mBindingSets, bs);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
