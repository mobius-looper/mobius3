/*
 * Model for a "track setup", a collection of parameters that apply to 
 * all tracks.
 *
 * This is still used in engine code so avoid changes until we can
 * finish porting that.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "ParameterConstants.h"
#include "Structure.h"

/****************************************************************************
 *                                                                          *
 *                                 CONSTANTS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * A special name that may be used for the Bindings property that
 * means to cancel the current binding overlay.  Normally a NULL value
 * here means "preserve the current overlay".
 */
#define SETUP_OVERLAY_CANCEL "cancel"

/****************************************************************************
 *                                                                          *
 *                                ENUMERATIONS                              *
 *                                                                          *
 ****************************************************************************/

extern const char* GetSyncSourceName(OldSyncSource src);

/****************************************************************************
 *                                                                          *
 *   								SETUP                                   *
 *                                                                          *
 ****************************************************************************/

class Setup : public Structure {

  public:

    Setup();
    Setup(Setup* src);
    ~Setup();

    // Structure downcast
    Setup* getNextSetup() {
        return (Setup*)getNext();
    }
    
    Structure* clone();
    
    // put it it the standard state for the unit tests
    void reset(class Preset* p);

	void setDefaultPresetName(const char* preset);
	const char* getDefaultPresetName();
    
	void setBindings(const char* name);
	const char* getBindings();

	void setActiveTrack(int i);
	int getActiveTrack();

	void setResetRetains(const char* csv);
	const char* getResetRetains();
	//bool isResetRetain(const char* parameterName);

	class SetupTrack* getTracks();
	class SetupTrack* stealTracks();
	void setTracks(SetupTrack* list);
	SetupTrack* getTrack(int index);

    // Sync options

    OldSyncSource getSyncSource();
    void setSyncSource(OldSyncSource src);

    OldSyncUnit getSyncUnit();
    void setSyncUnit(OldSyncUnit unit);

    SyncTrackUnit getSyncTrackUnit();
    void setSyncTrackUnit(SyncTrackUnit unit);

    bool isManualStart();
    void setManualStart(bool b);

	void setMinTempo(int t);
	int getMinTempo();
	void setMaxTempo(int t);
	int getMaxTempo();
	void setBeatsPerBar(int t);
	int getBeatsPerBar();

	void setMuteSyncMode(MuteSyncMode m);
	void setMuteSyncMode(int m);
	MuteSyncMode getMuteSyncMode();

	void setResizeSyncAdjust(SyncAdjust i);
	void setResizeSyncAdjust(int i);
	SyncAdjust getResizeSyncAdjust();

	void setSpeedSyncAdjust(SyncAdjust i);
	void setSpeedSyncAdjust(int i);
	SyncAdjust getSpeedSyncAdjust();

	void setRealignTime(RealignTime sm);
	void setRealignTime(int i);
	RealignTime getRealignTime();

	void setOutRealignMode(OutRealignMode m);
	void setOutRealignMode(int i);
	OutRealignMode getOutRealignMode();

  private:

	void initParameters();

	/**
	 * Currently active track.
	 */
	int mActiveTrack;

    /**
     * Base Preset for all tracks.
     */
	char* mDefaultPresetName;
    
	/**
	 * User defined group names. (not used yet)
	 */
	//StringList* mGroupNames;

	/**
	 * List of track parameter names that will NOT be restored from the
	 * setup after an individual (non-global) reset.
	 */
	char* mResetRetains;

	/**
	 * A list of track configurations.
	 */
	class SetupTrack* mTracks;

	/**
	 * Currently overlay BindingSet
	 */
	char* mBindings;

    //
    // Synchronization
    //

    /**
     * The default sync source for all tracks.
     */
    OldSyncSource mSyncSource;

    /**
     * The sync unit for MIDI and HOST.
     * Tracks can't override this, should they be able to?
     */
    OldSyncUnit mSyncUnit;

    /**
     * The default track sync unit for all tracks.
     */
    SyncTrackUnit mSyncTrackUnit;

    /**
     * True if an MS_START message should be sent manually rather
     * than automatically when the loop closes during SYNC_OUT.
     * Formerly SyncMode=OutUserStart
     */
    bool mManualStart;

	/**
	 * The minimum tempo we will try to use in SYNC_OUT.
	 */
	int mMinTempo;

	/**
	 * The maximum tempo we will try to use in SYNC_OUT.
	 */
	int mMaxTempo;

    /**
     * The number of beats in a bar.
     * If not set we'll attempt to get it from the host when host
     * syncing, then fall back to the Preset::8thsPerCycle.
     *
     * This is used in SYNC_OUT to define the cycle size (really?)
     * This is used in SYNC_MIDI to define the bar widths for
     * quantization.
     */
    int mBeatsPerBar;

    /**
     * During SYNC_OUT, determines how MIDI transport and clocks 
     * are sent during mute modes Start and Pause.
     */
    MuteSyncMode mMuteSyncMode;

    /**
     * During SYNC_OUT, specifies how unrounded cycle resizes are
     * supposed to be processed.
     */
    SyncAdjust mResizeSyncAdjust;

	/**
	 * During SYNC_OUT, specifies how rate changes are handled.
	 */
	SyncAdjust mSpeedSyncAdjust;

	/**
	 * Controls the time at which a Realign occurs.
	 */
	RealignTime mRealignTime;

	/**
	 * When using SYNC_OUT, determines how to realign the external sync loop.
	 */
	OutRealignMode mOutRealignMode;


};

/****************************************************************************
 *                                                                          *
 *                                SETUP TRACK                               *
 *                                                                          *
 ****************************************************************************/

/**
 * The state of one track in a Setup.
 */
class SetupTrack {

  public:

	SetupTrack();
	SetupTrack(SetupTrack* src);
	~SetupTrack();
	// SetupTrack* clone();
	void reset();

    // will need to sort this out
	//void capture(class MobiusState* state);

	void setNext(SetupTrack* n);
	SetupTrack* getNext();

	void setName(const char* name);
	const char* getName();

	void setTrackPresetName(const char* preset);
	const char* getTrackPresetName();

    // old way with ordinal
    void setGroupNumberDeprecated(int i);
    int getGroupNumberDeprecated();
    // new way with name
    void setGroupName(juce::String name);
    juce::String getGroupName();

	void setFocusLock(bool b);
	bool isFocusLock();

	void setInputLevel(int i);
	int getInputLevel();

	void setOutputLevel(int i);
	int getOutputLevel();

	void setFeedback(int i);
	int getFeedback();

	void setAltFeedback(int i);
	int getAltFeedback();

	void setPan(int i);
	int getPan();

	void setMono(bool b);
	bool isMono();

	void setAudioInputPort(int i);
	int getAudioInputPort();
	
	void setAudioOutputPort(int i);
	int getAudioOutputPort();

	void setPluginInputPort(int i);
	int getPluginInputPort();
	
	void setPluginOutputPort(int i);
	int getPluginOutputPort();

    OldSyncSource getSyncSource();
    void setSyncSource(OldSyncSource src);
    SyncTrackUnit getSyncTrackUnit();
    void setSyncTrackUnit(SyncTrackUnit unit);

    class UserVariables* getVariables();
    void setVariables(UserVariables* vars);
    
	void setVariable(const char* name, class ExValue* value);
	void getVariable(const char* name, class ExValue* value);

  private:

	SetupTrack* mNext;
	char* mName;
	char* mTrackPresetName;
	bool mFocusLock;
	bool mMono;
    int mGroup;
    juce::String mGroupName;
	int mInputLevel;
	int mOutputLevel;
	int mFeedback;
	int mAltFeedback;
	int mPan;
	int mAudioInputPort;
	int mAudioOutputPort;
	int mPluginInputPort;
	int mPluginOutputPort;

    // Sync overrides

    OldSyncSource mSyncSource;
    SyncTrackUnit mSyncTrackUnit;

	/**
	 * User defined variable saved with the track.
	 */
	class UserVariables* mVariables;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
