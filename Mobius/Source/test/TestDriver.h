/**
 * An evolving system to run Mobius engine tests.
 *
 */

#pragma once

#include "../mobius/MobiusInterface.h"

#include "TestPanel.h"

/**
 * The size of the two interleaved sample buffers we simulate.
 * The block size is fixed at 256, but make these large enough that
 * we can play with the block size without having to dynamically allocate these.
 *
 * Note that this will end up on the stack since Supervisor has TestDriver
 * as a member object.  These plus JuceAudiostream's buffers eat up quite a bit of
 * stack space.  Might want to dynamically allocate these.
 */
const int TestDriverMaxFramesPerBuffer = 1024;
const int TestDriverMaxChannels = 2;
const int TestDriverMaxSamplesPerBuffer = TestDriverMaxFramesPerBuffer * TestDriverMaxChannels;

class TestDriver : public MobiusListener, public MobiusAudioListener, public MobiusAudioStream
{
    friend class TestPanel;
    
  public:

    TestDriver(class Supervisor* super);
    ~TestDriver();

    void initialize(juce::Component* parent);
    void start();
    void stop();
    bool isActive();

    void advance();

    void captureConfiguration(class UIConfig* config);

    // MobiusListener
	void mobiusTimeBoundary() override;
    void mobiusEcho(juce::String) override;
    void mobiusMessage(juce::String) override;
    void mobiusAlert(juce::String) override;
    void mobiusDoAction(class UIAction*) override;
    void mobiusPrompt(class MobiusPrompt*) override;
    void mobiusMidiReceived(juce::MidiMessage& msg) override;
    void mobiusDynamicConfigChanged() override;
    void mobiusTestStart(juce::String) override;
    void mobiusTestStop(juce::String) override;
    void mobiusSaveAudio(Audio*, juce::String) override;
    void mobiusSaveCapture(Audio*, juce::String) override;
    void mobiusDiff(juce::String, juce::String, bool reverse) override;
    void mobiusDiffText(juce::String, juce::String) override;
    Audio* mobiusLoadAudio(juce::String) override;
    void mobiusScriptFinished(int requestId) override;

    // MobiusAudioListener
    void processAudioStream(class MobiusAudioStream* stream) override;

    // MobiusAudioStream
	long getInterruptFrames() override;
	void getInterruptBuffers(int inport, float** input, 
                                     int outport, float** output) override;

    juce::MidiBuffer* getMidiMessages() override;
    class MobiusMidiTransport* getMidiTransport() override;

    double getStreamTime() override;
    double getLastInterruptStreamTime() override;
    class AudioTime* getAudioTime() override;

    // serious hackery for AudioDifferencer
    class AudioPool* getAudioPool();

    // so TestPanel can send actions
    class MobiusInterface* getMobius();


    

  protected:
    
    void controlPanelClosed();
    void reinstall();
    void setBypass(bool b);
    void runTest(class Symbol* s, juce::String testName);
    void cancel();
    
  private:

    class Supervisor* supervisor;
    TestPanel controlPanel {this};
    
    // true when we're in control
    bool active = false;
    // true when we've installed the test configuration
    bool installed = false;
    // true when we're in audio stream bypass mode
    bool bypass = false;

    // requestId generator for test script tracking
    int requestIdCounter = 1;
    // script we're waiting on
    int waitingId = 0;
    // time we started waiting
    int waitStart = 0;
    // development hack
    int checkMemoryLeak = 0;
    
    // the two control chains we insert ourselves into
    class MobiusListener* defaultMobiusListener = nullptr;
    class MobiusAudioListener* defaultAudioListener = nullptr;

    // the two simulated input and output buffers
    float dummyInputBuffer[TestDriverMaxSamplesPerBuffer];
    float dummyOutputBuffer[TestDriverMaxSamplesPerBuffer];

    class MobiusShell* getMobiusShell();
    void installTestConfiguration();
    void installPresetAndSetup(class MobiusConfig* config);
    class MobiusConfig* readConfigOverlay();
    juce::File getTestRoot();
    juce::File getResultFile(juce::String name);
    juce::File addExtensionWav(juce::File file);
    juce::File getExpectedFile(juce::String name);
    juce::File followRedirect(juce::File root);
    juce::String findRedirectLine(juce::String src);
    void diffText(juce::File result, juce::File expected);

    void pumpBlocks();
    void pumpBlock();
    void doTestAnalysis();
    void avoidMemoryLeak();
    
};
