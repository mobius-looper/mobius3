/**
 * Interface of an object that can process Juce audio "streams"
 * send in either standalone mode for an AudioAppComponent or
 * in plugin mode when for an AudioProcessor.
 *
 * There are two implementations: JuceMobiusContainer which is installed
 * during normal operation, and TestMobiusContainer which is active during test mode.
 *
 * One of these will given to either MainComponent or PluginProcessor by Supervisor
 * when it initializes and when test mode is activated.
 */

#include <JuceHeader.h>

class AudioStreamHandler
{
  public:

    // standalone audio thread callbacks
    virtual void prepareToPlay (int samplesPerBlockExpected, double sampleRate) = 0;
    virtual void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) = 0;
    virtual void releaseResources() = 0;

    // plugin audio thread callbacks
    virtual void prepareToPlayPlugin(double sampleRate, int samplesPerBlock) = 0;
    virtual void releaseResourcesPlugin() = 0;
    virtual void processBlockPlugin(juce::AudioBuffer<float>&, juce::MidiBuffer&) = 0;

};
