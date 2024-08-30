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
#include "GroupDefinition.h"

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
#define MSEC_TO_FRAMES(msec) (int)(CD_SAMPLE_RATE * ((float)msec / 1000.0f))

#define DEFAULT_EVENT_GRAVITY_FRAMES MSEC_TO_FRAMES(DEFAULT_EVENT_GRAVITY_MSEC)

/**
 * The EDP automatically applies around a 5% feedback reduction when
 * overdubbing and feedback is at 100%, in order to help avoid overload.
 * 95% of 128 is 121.6.  This is also the amount of feedback reduction
 * we have to go beyond in order to force a layer shift if no new content
 * was overdubbed.
 *
 * !! Should be in global configuration
 */
#define AUTO_FEEDBACK_LEVEL 121

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

/****************************************************************************
 *                                                                          *
 *                                ENUMERATIONS                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Values for the driftCheckPoint parameter.
 * Made this an enumeration instead of a boolean in case we
 * want to introduce more granular check points like DRIFT_CHECK_CYCLE
 * or even DRIFT_CHECK_SUBCYCLE.  Seems like overkill though.
 */
typedef enum {

	// check at the Mobius loop start point
	DRIFT_CHECK_LOOP,

	// check at the external loop start point
	DRIFT_CHECK_EXTERNAL

} DriftCheckPoint;

/**
 * Values for the midiRecordMode paramter.
 * This an internal parameter used for experimenting with styles
 * of calculating the optimal loop length when using MIDI sync.
 * The default is MIDI_AVERAGE_TEMPO and this should not normallyu
 * be changed.  Once we've had some time to experiment with these
 * options in the field, this should be removed and hard coded into
 * Synchronizer.
 */
typedef enum {

    // average tempo calculated by MidiInput
    MIDI_TEMPO_AVERAGE,

    // smooth tempo calculated by MidiInput, accurate to 1/10th BPM
    MIDI_TEMPO_SMOOTH,

    // end exactly on a MIDI clock pulse
    MIDI_RECORD_PULSED

} MidiRecordMode;

/**
 * Sample rate could be an integer, but it's easier to prevent
 * crazy values if we use an enumeration.
 */
typedef enum {

	SAMPLE_RATE_44100,
	SAMPLE_RATE_48000

} AudioSampleRate;

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

    MobiusConfig* clone();

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

	void setTracks(int i);
	int getTracks();
	void setTrackGroups(int i);
	int getTrackGroups();
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
#endif
    
	void setTracePrintLevel(int i);
	int getTracePrintLevel();
	void setTraceDebugLevel(int i);
	int getTraceDebugLevel();

	void setSaveLayers(bool b);
	bool isSaveLayers();

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
    
	class BindingSet* getBindingSets();
	void addBindingSet(class BindingSet* bs);
    void setBindingSets(BindingSet* list);

    class ScriptConfig* getScriptConfig();
    void setScriptConfig(class ScriptConfig* c);
    
	void setSampleConfig(class SampleConfig* s);
	class SampleConfig* getSampleConfig();

	void setQuickSave(const char* s);
	const char* getQuickSave();

	void setFocusLockFunctions(class StringList* functions);
	StringList* getFocusLockFunctions();

	void setMuteCancelFunctions(class StringList* functions);
	class StringList* getMuteCancelFunctions();

	void setConfirmationFunctions(class StringList* functions);
	class StringList* getConfirmationFunctions();

	void setAltFeedbackDisables(class StringList* functions);
	class StringList* getAltFeedbackDisables();

	void setLongPress(int msecs);
	int getLongPress();

	void setDriftCheckPoint(DriftCheckPoint p);
	DriftCheckPoint getDriftCheckPoint();

	void setMidiRecordMode(MidiRecordMode m);
	MidiRecordMode getMidiRecordMode();

    void setDualPluginWindow(bool b);
    bool isDualPluginWindow();

    void setMidiExport(bool b);
    bool isMidiExport();

    void setHostMidiExport(bool b);
    bool isHostMidiExport();

    void setGroupFocusLock(bool b);
    bool isGroupFocusLock();

    void setNoSyncBeatRounding(bool b);
    bool isNoSyncBeatRounding();

    void setEdpisms(bool b);
    bool isEdpisms();

    juce::OwnedArray<class GroupDefinition> groups;
    int getGroupOrdinal(juce::String name);
    
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
	int mTracks;
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

	class BindingSet* mBindingSets;
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
     * Determines how we calculate the ending loop length with
     * using SYNC_MIDI.
	 */
	MidiRecordMode mMidiRecordMode;

    /**
     * When true, enables dual plugin windows where the window given
     * to us by the host is used a small launch pad to bring up the main 
     * window.  When false, we force the UI into the host window which
     * must have been presized for VST, or adaptable for AU.
     * 
     * This is ignored for Mac (both AU and VST) since I couldn't get
     * it working properly and is generally not desired.  It was
     * historically forced on for Windows but is now configurable.
     */
    bool mDualPluginWindow;

    /**
     * When true, parameters and controls that are bound to MIDI
     * continuous control events will have a corresponding event
     * sent to the MIDI output device whenever the parameter/control
     * changes.  This is used with bi-directional control surfaces
     * to track changes to the control made in the UI or a script.
     */
    bool mMidiExport;

    /**
     * Like mMidiExport except that the tracking messages are
     * sent to the VST or AU host and routed as appropriate in the host.
     * This is usuallky an alternate to opening a MIDI output device
     * for feedabck, though both can be used at the same time.
     */
    bool mHostMidiExport;

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
