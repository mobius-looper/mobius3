/**
 * Old and very basic configuration object containing a list of files that
 * can be loaded and played as one-shot samples.  Used for my testing, but
 * not used often by users.
 *
 * The SampleConfig saved in config files has only the path name to the
 * data files.  At runtime the SampleConfig is "loaded" by reading the
 * sample data and saving it temporarily in each Sample object.  The entire
 * package is then sent to the kernel for installation in the sample player.
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

#include <JuceHeader.h>

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
 * One of these will be the SystemConfig.
 */
class SampleConfig
{
  public:

    constexpr static const char* XmlName = "SampleConfig";
    
	SampleConfig();
	SampleConfig(SampleConfig* src);
	~SampleConfig();

	void clear();
	void add(class Sample* s);

    juce::OwnedArray<Sample>& getSamples();
    
    void toXml(juce::XmlElement* root);
    void parseXml(juce::XmlElement* root, juce::StringArray& errors);
    
  private:
    
    juce::OwnedArray<Sample> samples;
	
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

    constexpr static const char* XmlName = "Sample";
    
	Sample();
	Sample(Sample* src);
	Sample(juce::String file);
	~Sample();

    juce::String file;

    void setData(float* data, int frames);
    float* getData();
    int getFrames();


    // playback characteristics
    // these were never used, and shouldn't be at this level anyway
    // how the sample is played should be under the control
    // by something higher, possibly under ad-hoc user control

	/**
	 * When true, playback continues only as long as the trigger
	 * is sustained.  When false, the sample always plays to the end
	 * and stops.
	 */
    bool sustain = false;
    
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
    bool loop = false;
    
    /**
     * When true, multiple overlapping playbacks of the sample
     * are allowed.  This is really meaningful only when mSustain 
     * is false since you would have to have an up transition before
     * another down transition.  But I suppose you could configure
     * a MIDI controller to do that.  This is also what you'd want
     * to implement pitch adjusted playback of the same sample.
     */
    bool concurrent = false;
    
    // hack for testing so these can be like Scripts
    bool button = false;
    
    void toXml(juce::XmlElement* root);
    void parseXml(juce::XmlElement* root, juce::StringArray& errors);
    
  private:

    // there are better Juce containers for this, but this gets the job
    // done util the concept needs to get more complicated
    float* data = nullptr;
    int frames = 0;
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
