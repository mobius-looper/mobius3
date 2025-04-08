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

#ifndef MOBIUS_CONFIG_H
#define MOBIUS_CONFIG_H

#include <JuceHeader.h>
#include "../GroupDefinition.h"
#include "../ParameterConstants.h"

/****************************************************************************
 *                                                                          *
 *                                 CONSTANTS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Default message catalog language.
 */
#define DEFAULT_LANGUAGE "USEnglish" 

/**
 * Default number of Mobius tracks.
 */
#define DEFAULT_TRACKS 8

/**
 * Default number of track groups.
 */
#define DEFAULT_TRACK_GROUPS 2

/**
 * Default maximum loops per track.
 */
#define DEFAULT_MAX_LOOPS 4

/**
 * Default noise floor.
 */
#define DEFAULT_NOISE_FLOOR 13

/**
 * Default input latency adjustments.
 */
#define DEFAULT_INPUT_LATENCY 0
#define DEFAULT_OUTPUT_LATENCY 0

/**
 * Default number of frames we'll allow the loop to drift away
 * from a sync pulse before correcting.
 */
#define DEFAULT_MAX_SYNC_DRIFT 2048

/**
 * The default number of milliseconds in a long press.
 */
#define DEFAULT_LONG_PRESS_MSECS 500

/**
 * Default number of frames to use when computing event "gravity".
 * If an event is within this number of frames after a quantization boundary,
 * we will quantize back to that boundary rather than ahead to the next one.
 * Doc say things like "a few hundred milliseconds" and "150ms" so let's
 * interpret that as 2/10 second.
 * NOTE: This is not actually used.
 *
 * !! Should be in global configuration
 */
#define DEFAULT_EVENT_GRAVITY_MSEC 200

/**
 * Calculate the number of frames in a millisecond range.
 * NOTE: Can't actually do it this way since sample rate is variable,
 * need to calculate this at runtime based on the stream and cache it!
 */
//#define MSEC_TO_FRAMES(msec) (int)(CD_SAMPLE_RATE * ((float)msec / 1000.0f))

//#define DEFAULT_EVENT_GRAVITY_FRAMES MSEC_TO_FRAMES(DEFAULT_EVENT_GRAVITY_MSEC)

/**
 * The maximum number of track groups we allow.
 * !! Should be in global configuration
 */
#define MAX_TRACK_GROUPS 4

/**
 * The maximum number of tracks that can be assigned direct channels.
 * !! Should be in global configuration
 */
#define MAX_CHANNEL_TRACKS 8

/**
 * Maximum range for pitch and rate shift in chromatic steps.
 * This is semitones in one direction so 48 is four octaves up 
 * and down.  This should be consistent with Resampler::MAX_RATE_OCTAVE.
 */
#define MAX_SPREAD_RANGE 48 

/**
 * Default range for pitch and rate shift in chromatic steps.
 */
#define DEFAULT_SPREAD_RANGE 48

/**
 * Default number of LayerInfo objects returned in a MobiusState.
 * This also controls the width of the layer list in the UI.
 */
#define DEFAULT_MAX_LAYER_INFO 20

/**
 * Default number of LayerInfo objects returned in a MobiusStat
 * to represent redo layers.
 * This also controls the width of the layer list in the UI.
 */
#define DEFAULT_MAX_REDO_INFO 10

/**
 * Size of a static char buffer to keep the custom mode name.
 */
#define MAX_CUSTOM_MODE 256

/****************************************************************************
 *                                                                          *
 *                               MOBIUS CONFIG                              *
 *                                                                          *
 ****************************************************************************/

class MobiusConfig {
    
  public:

    MobiusConfig();
    MobiusConfig(bool dflt);
    ~MobiusConfig();

    MobiusConfig* clone(class SymbolTable* st);

    // two transient flags to enable optimizations when
    // reconfiguring the engine after editing the entire MobiusConfig
    bool setupsEdited = false;
    bool presetsEdited = false;

    int getVersion() {
        return mVersion;
    }

    void setVersion(int v) {
        mVersion = v;
    }
    
    // !! wtf does this do?
	void generateNames();
	
    const char* getError();
    //MobiusConfig* clone();

    bool isDefault();
    void setHistory(MobiusConfig* config);
    MobiusConfig* getHistory();
    int getHistoryCount();

    // new stuff
    int mControllerActionThreshold = 0;
    
	void setMonitorAudio(bool b);
	bool isMonitorAudio();
	void setHostRewinds(bool b);
	bool isHostRewinds();
	void setAutoFeedbackReduction(bool b);
	bool isAutoFeedbackReduction();
	void setIsolateOverdubs(bool b);
	bool isIsolateOverdubs();
	void setIntegerWaveFile(bool b);
	bool isIntegerWaveFile();
	void setSpreadRange(int i);
	int getSpreadRange();

	void setCoreTracks(int i);
	int getCoreTracksDontUseThis();
	void setTrackGroupsDeprecated(int i);
	int getTrackGroupsDeprecated();
	void setMaxLoops(int i);
	int getMaxLoops();

	void setNoiseFloor(int i);
	int getNoiseFloor();

	void setInputLatency(int i);
	int getInputLatency();
	void setOutputLatency(int i);
	int getOutputLatency();

	void setFadeFrames(int i);
	int getFadeFrames();

	void setMaxSyncDrift(int i);
	int getMaxSyncDrift();

	void setQuickSave(const char* s);
	const char* getQuickSave();

	void setSaveLayers(bool b);
	bool isSaveLayers();

	void setLongPress(int msecs);
	int getLongPress();

	void setDriftCheckPoint(DriftCheckPoint p);
	DriftCheckPoint getDriftCheckPoint();

    void setGroupFocusLock(bool b);
    bool isGroupFocusLock();

    void setNoSyncBeatRounding(bool b);
    bool isNoSyncBeatRounding();

    void setEdpisms(bool b);
    bool isEdpisms();

#if 0
	const char* getMidiInput();
	void setMidiInput(const char* s);
	const char* getMidiOutput();
	void setMidiOutput(const char* s);
	const char* getMidiThrough();
	void setMidiThrough(const char* s);

	const char* getPluginMidiInput();
	void setPluginMidiInput(const char* s);
	const char* getPluginMidiOutput();
	void setPluginMidiOutput(const char* s);
	const char* getPluginMidiThrough();
	void setPluginMidiThrough(const char* s);

	const char* getAudioInput();
	void setAudioInput(const char* s);
	const char* getAudioOutput();
	void setAudioOutput(const char* s);
    void setSampleRate(AudioSampleRate rate);
    AudioSampleRate getSampleRate();

	void setTracePrintLevel(int i);
	int getTracePrintLevel();
	void setTraceDebugLevel(int i);
	int getTraceDebugLevel();
    
#endif

    // sigh, use of this is buried at levels that make access
    // to Grouper harder, weed those out
    juce::OwnedArray<class GroupDefinition> dangerousGroups;
    
    juce::OwnedArray<class GroupDefinition>& getGroupDefinitions() {
        return dangerousGroups;
    }
    
    int getGroupOrdinal(juce::String name);
    
	class Setup* getSetups();
	void setSetups(class Setup* list);
	void addSetup(class Setup* p);
    class Setup* getSetup(const char* name);
    class Setup* getSetup(int ordinal);

    const char* getStartingSetupName();
    void setStartingSetupName(const char* name);
    class Setup* getStartingSetup();
    
	class Preset* getPresets();
    void setPresets(class Preset* list);
	void addPreset(class Preset* p);
    class Preset* getPreset(const char* name);
    class Preset* getPreset(int ordinal);
    class Preset* getDefaultPreset();
    
	class OldBindingSet* getBindingSets();
	void addBindingSet(class OldBindingSet* bs);
    void setBindingSets(OldBindingSet* list);

    class ScriptConfig* getScriptConfigObsolete();
    void setScriptConfigObsolete(class ScriptConfig* c);
    
	void setSampleConfig(class SampleConfig* s);
	class SampleConfig* getSampleConfig();

	void setFocusLockFunctions(class StringList* functions);
	StringList* getFocusLockFunctions();

	void setMuteCancelFunctions(class StringList* functions);
	class StringList* getMuteCancelFunctions();

	void setConfirmationFunctions(class StringList* functions);
	class StringList* getConfirmationFunctions();

	void setAltFeedbackDisables(class StringList* functions);
	class StringList* getAltFeedbackDisables();

    
    //
    // Transient fields for testing
    //

	void setUnitTests(const char* s);
	const char* getUnitTests();

    void setNoSetupChanges(bool b);
    bool isNoSetupChanges();
    
    void setNoPresetChanges(bool b);
    bool isNoPresetChanges();

  private:

	void init();
    //void generateNames(class Bindable* bindables, const char* prefix, 
    //const char* baseName);
    //void numberThings(class Bindable* things);
    //int countThings(class Bindable* things);

    int mVersion = 1;
    char mError[256];
    bool mDefault;
    MobiusConfig* mHistory;
	char* mQuickSave;

	/**
	 * The noise floor sample level.
	 * If the absolute values of the 16-bit samples in a recorded loop
	 * are all below this number, then the loop is considered to have
	 * no content.  Used to reduce the number of overdub loops we keep
	 * around for undo.  Typical values are 10-13 which correspond to
	 * float sample values from 0.000305 to 0.0004.
	 */
	int mNoiseFloor;

	int mInputLatency;
	int mOutputLatency;
	int mFadeFrames;
	int mMaxSyncDrift;
	int mCoreTracks;
    int mTrackGroups;
    int mMaxLoops;
	int mLongPress;

	class StringList* mFocusLockFunctions;
	class StringList* mMuteCancelFunctions;
	class StringList* mConfirmationFunctions;
	class StringList* mAltFeedbackDisables;

    /**
     * We have a list of setups and one is considered active.
     * The setup may change dynamically as Mobius runs but if you
     * edit the setup configuration it will revert to the one that
     * was selected when the config was saved. 
     */
	class Setup* mSetups;
    char* mStartingSetupName;
    class Setup* mStartingSetup;

	class Preset* mPresets;

	class OldBindingSet* mBindingSets;
    char* mBindings;
    char* mBindingOverlays;

    class ScriptConfig* mScriptConfig;
	class SampleConfig* mSampleConfig;
    

	// class ControlSurfaceConfig* mControlSurfaces;

    /**
     * Sample rate for both input and output streams.
     */
//    AudioSampleRate mSampleRate;

	/**
	 * When true, audio input is passed through to the audio output 
	 * for monitoring.  This is only effective if you are using
	 * low latency drivers.
	 */
	bool mMonitorAudio;

	/**
	 * When true, the host may rewind slightly immediately after
	 * starting so we have to defer detection of a bar boundary.
	 */
	bool mHostRewinds;

	/**
	 * When true, indicates that we should perform an automatic
	 * 5% reduction in feedback during an overdub.  The EDP does this,
	 * but it makes the flattening vs. non flattening tests behave differently
	 * so we need a way to turn it off.
	 */
	bool mAutoFeedbackReduction;

	/**
	 * When true we save a copy of just the new content added to each layer
     * as well as maintaining the flattened layer.  This is then saved in the
     * project so you can process just the overdub.  This was an experimental
     * feature added around the time layer flattening was introduced.  It is 
     * no longer exposed in the user interface because it's hard to explain,
     * it isn't obvious when it has been enabled, and it can up to double
     * the amount of memory required for each layer.  
	 */
	bool mIsolateOverdubs;

	/**
	 * True if we're supposed to save loop and project wave files
	 * using 16 bit PCM encoding rather than IEEE floats.
	 */
	bool mIntegerWaveFile;

	/**
	 * The maximum number of semitones of speed or pitch shift when
     * SpeedStep or PitchStep is bound to a MIDI note or program change
     * trigger.  This is the number of semitones in one direction so 12
     * means an octave up and down.
	 */
	int mSpreadRange;

	/**
	 * Trace records at this level or lower are printed to the console.
	 */
	int mTracePrintLevel;

	/** 
	 * Trace records at this level or lower are sent to the debug output stream
	 */
	int mTraceDebugLevel;

	/**
	 * Controls whether we save the complete Layer history when
	 * saving a project.
	 */
	bool mSaveLayers;

	/**
	 * Specifies where we check for sync drift.
	 */
	DriftCheckPoint mDriftCheckPoint;

    /**
     * When true, track groups have focus lock.  This means
     * that a trigger with a global binding that is received
     * by a track will also be received by all tracks in the same 
     * group.  This was the behavior prior to 1.43, but is now an
     * option disabled by default.  
     */
    bool mGroupFocusLock;


    // why was this commented out?
#if 0
    /**
     * Maximum number of LayerInfo structures returned in a MobiusState.
     * Besides controlling how much we have to assemble, the UI
     * uses this to restict the size of the layer list component.
     * With high values, the layer list can extend beyond the end
     * of the screen.
     *
     * !! This needs to be redesigned.  Since layers don't change
     * often, we can just maintain info structures and return
     * the same ones every time rather than assembling them.  
     * How this is rendered should be a UI configuration option, 
     * not a global parameter.
     */
    int mMaxLayerInfo;

    /**
     * Maximum number of LayerInfo structures representing redo
     * layers returned in a MobiusState.
     * See mMaxLayerInfo for reasons I don't like this.
     */
    int mMaxRedoInfo;
#endif

    // Flags used to optimize the propagation of configuration changes
    // It is important to avoid propagating Preset and Setups if nothing
    // was changed to avoid canceling any temporary parameter values
    // maintained by the tracks.   I don't really like this...
    bool mNoPresetChanges;
    bool mNoSetupChanges;

    /**
     * Disable beat size rounding by the synchronizer.
     * Normally when calculating the size of a "beat" for synchronization
     * we like it to be an even integer so that that anything slaving
     * to beats will always be an exact multiple of the beat.
     * This is better for inter-track sync but may result in more
     * drift relative to the sync source.  This flag disables the
     * rounding.  It is intended only for experimentation and is not
     * exposed.
     */
    bool mNoSyncBeatRounding;

    /**
     * Enable a few EDPisms:
     *  Mute+Multiply = Realign
     *  Mute+Insert = RestartOnce (aka SamplePlay)
     *  Reset+Mute = previous preset
     *  Reset+Insert = next preset
     */
    bool mEdpisms;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
