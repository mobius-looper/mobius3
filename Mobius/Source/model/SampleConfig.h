/**
 * This is a refactoring of the original Sample model to make a clean
 * seperation between the UI and the engine.
 *
 * Conceptually a sample is a block of audio that may be played by the engine.
 * Normally as a "one shot" from beginning to end.
 *
 * The UI normally loads samples from the file system, then gives them
 * to the engine to be managed.
 *
 * The Sample object encapsulates these things:
 *
 *    - the path to the file where the sample audio is read from
 *    - an optional block of audio data, represented as a dynamically allocated
 *      array of float values
 *
 * A Sample may be in two states: loaded (has data) or unloaded (has only a filename).
 *
 * A SampleConfig contains a number of unloaded Samples.
 * This is what we store in the a configuration file and edit in the UI.
 * This was formerly called just "Samples".
 *
 * During initialization or when requested by the user, the SampleConfig
 * is read and all of the Samples it contain are loaded.  The loaded Samples
 * are then given to the Mobius engine.
 *
 * Currently the SampleConfig is stored within the MobiusConfig object and
 * in the mobius.xml file.  Since this is now just a UI concept it can
 * be moved to ui.xml.
 *
 * NOTES:
 *
 * I wanted use Audio out here but it just drags too much along with it related
 * to pooling and cursoring.  It is extremely sensitive core code that can't
 * be redesigned right now.  Unfortunately that means we lose the notion of
 * dividing the audio data into multiple blocks.  That could be added but
 * for now we'll just assume samples are of reasonable size and can be allocated
 * in a single heap block.
 *
 */

#pragma once

/**
 * Special prefix that may be added to the front of a sample file path
 * to indiciate that the full path is formed by the remainder of the
 * sample file path appended to the root path of the Mobius installation.
 * Used for a few demo samples inlcuded with the installation that
 * can't predict what the full paths will be after install.
 */
#define InstallationPathPrefix "$INSTALL"

//////////////////////////////////////////////////////////////////////
//
// SampleConfig
//
//////////////////////////////////////////////////////////////////////

/**
 * Encapsulates a collection of Samples for configuration storage.
 * One of these can be the MoibusConfig as well as local to a Project.
 */
class SampleConfig
{
  public:

	SampleConfig();
	~SampleConfig();

	void clear();
	void add(class Sample* s);

	Sample* getSamples();
    void setSamples(Sample* list);
    
  private:
    
	class Sample* mSamples;
	
};

//////////////////////////////////////////////////////////////////////
//
// Sample
//
//////////////////////////////////////////////////////////////////////

/**
 * The definition of a sample that can be played by the engine.
 * A list of these will be found in a Samples object which in turn
 * will be in the MobiusConfig.
 *
 * We convert these to SamplePlayers managed by one SampleManager.
 */
class Sample
{
  public:

	Sample();
	Sample(Sample* src);
	Sample(const char* file);
	~Sample();

	void setNext(Sample* s);
	Sample* getNext();

	void setFilename(const char* file);
	const char* getFilename();

    // playback characteristics
    // these were never used, and shouldn't be at this level anyway
    // how the sample is played should be under the control
    // by something higher, possibly under ad-hoc user control

	void setSustain(bool b);
	bool isSustain();

	void setLoop(bool b);
	bool isLoop();

    void setConcurrent(bool b);
    bool isConcurrent();

    float* getData();
    int getFrames();
    
    void setData(float* data, int frames);

    // hack for testing so these can be like Scripts
    void setButton(bool b) {
        mButton = b;
    }

    bool isButton() {
        return mButton;
    }

  private:
	
	void init();

	Sample* mNext;
	char* mFilename;

    //
    // NOTE: These were experimental options that have never
    // been used.  It doesn't feel like these should even be
    // Sample-specific options but maybe...
    // 

	/**
	 * When true, playback continues only as long as the trigger
	 * is sustained.  When false, the sample always plays to the end
	 * and stops.
	 */
	bool mSustain;

	/** 
	 * When true, playback loops for as long as the trigger is sustained
	 * rather than stopping when the audio ends.  This is relevant
	 * only if mSustain is true.
     *
     * ?? Is this really necessary, just make this an automatic
     * part of mSustain behavior.  It might be fun to let this
     * apply to non sustained samples, but then we'd need a way
     * to shut it off!  Possibly the down transition just toggles
     * it on and off.
	 */
	bool mLoop;

    /**
     * When true, multiple overlapping playbacks of the sample
     * are allowed.  This is really meaningful only when mSustain 
     * is false since you would have to have an up transition before
     * another down transition.  But I suppose you could configure
     * a MIDI controller to do that.  This is also what you'd want
     * to implement pitch adjusted playback of the same sample.
     */
    bool mConcurrent;

    /**
     * Optional loaded sample data to pass to the engine.
     */
    float* mData;
    int mFrames;

    bool mButton;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
