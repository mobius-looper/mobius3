/**
 * Subcomponent of SyncMaster that analyzes synchronization state from the plugin host.
 *
 * This has somewhat unusual layering because it requires access to the juce::AudioProcessor
 * which can't easilly be abstracted away.  What it does is maintain HostSyncState and
 * AudioTime which are reasonably independent of Juce, though at this point the prospect
 * of moving to another plugin framework is unlikely.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "Pulse.h"

class HostAnalyzerV2
{
  public:

    HostAnalyzerV2();
    ~HostAnalyzerV2();

    void initialize(juce::AudioProcessor* ap);
    void setSampleRate(int rate);
    void advance(int blockFrames);

  private:

    juce::AudioProcessor* audioProcessor = nullptr;

    // this comes in weird, it was captured by JuceAudioStream
    // during the prepare notification, but seems like AudioProcessor should have
    // it too?
    int sampleRate = 44100;

    // things we derive from AudioProcessor
    float tempo = 0.0f;
    int timeSignatureNumerator = 0;
    int timeSignatureDenominator = 0;
    int beatsPerBar = 0;
    bool playing = false;
    int beat = 0;
    int bar = 0;

    // the pulse we export
    Pulse pulse;
    
    // hacks for host sync debugging
    bool traceppq = false;
    double lastppq = -1.0f;
    
};

