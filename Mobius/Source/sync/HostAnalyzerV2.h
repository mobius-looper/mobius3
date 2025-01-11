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

#include "SyncSourceResult.h"

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

    /**
     * The results of the analysis of each block.
     */
    SyncSourceResult result;
    
    //
    // Things we derive from the AudioProcessor
    //

    double tempo = 0.0f;
    // whether the tempo was given to us by the host or derived from beat distance
    bool tempoSpecified = false;
    int timeSignatureNumerator = 0;
    int timeSignatureDenominator = 0;
    bool playing = false;
    int beat = -1;
    long streamTime = 0;

    //
    // Locked unit and drift state
    //

    // once tempo lock has been achieved the length of the base unit
    // when this is zero, it means there is no tempo lock
    int unitLength = 0;

    // position within the tracking loop in the audio stream
    int streamFrame = 0;
    
    int streamUnits = 0;
    int streamUnitCounter = 0;
    int unitsPerBeat = 1;
    int streamBeat = 0;
    int streamBar = 0;
    int streamLoop = 0;
    
    void resync(double beatPosition);
    void advanceAudioStream(int blockFrames);

    //
    // State derived on each block when significant events happen
    //

    void ponderTempo(double newTempo);
    int tempoToUnit(double newTempo);
    void ponderPpq(double beatPosition, int blockSize);
    void deriveTempo(double beatPosition, int blockSize);

    //
    // Tempo monitoring from beats
    //

    double lastPpq = 0.0f;
    int lastPpqStreamTime = 0;
    void resetTempoMonitor();
    
    //
    // Trace options
    //
    
    bool traceppq = true;
    double lastppq = -1.0f;
    bool traceppqFine = false;
    int ppqCount = 0;
    
    void traceFloat(const char* format, double value);

};

