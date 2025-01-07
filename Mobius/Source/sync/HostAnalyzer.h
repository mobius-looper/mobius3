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

#include "HostSyncState.h"
#include "HostAudioTime.h"

class HostAnalyzer
{
  public:

    HostAnalyzer();
    ~HostAnalyzer();

    void initialize(juce::AudioProcessor* ap);
    void setSampleRate(int rate);
    void advance(int blockFrames);

    HostAudioTime* getAudioTime();

  private:

    juce::AudioProcessor* audioProcessor = nullptr;
    
    NewHostSyncState syncState;
    HostAudioTime audioTime;
    int sampleRate = 44100;
    
    // hacks for host sync debugging
    bool traceppq = false;
    double lastppq = -1.0f;
    
};

