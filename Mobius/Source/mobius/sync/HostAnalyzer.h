/**
 * Subcomponent of SyncMaster that analyzes synchronization state from the plugin host.
 *
 * This has somewhat unusual layering because it requires access to the juce::AudioProcessor
 * which can't easilly be abstracted away.  
 */

#pragma once

#include <JuceHeader.h>

#include "SyncAnalyzer.h"
#include "SyncAnalyzerResult.h"
#include "DriftMonitor.h"

class HostAnalyzer : public SyncAnalyzer
{
  public:

    HostAnalyzer();
    ~HostAnalyzer();

    void initialize(juce::AudioProcessor* ap);
    void setSampleRate(int rate);

    //
    // SyncAnalyzer Interface
    //

    void analyze(int blockFrames) override;
    SyncAnalyzerResult* getResult() override;
    bool isRunning() override;
    bool hasNativeBeat() override {return true;}
    int getNativeBeat() override;
    bool hasNativeBar() override;
    int getNativeBar() override;
    int getElapsedBeats() override;
    bool hasNativeTimeSignature() override;
    int getNativeBeatsPerBar() override;
    float getTempo() override;
    int getUnitLength() override;
    int getDrift() override;
    
  private:

    juce::AudioProcessor* audioProcessor = nullptr;

    // this comes in weird, it was captured by JuceAudioStream
    // during the prepare notification, but seems like AudioProcessor should have
    // it too?
    int sampleRate = 44100;

    // The results of the analysis of each block.
    SyncAnalyzerResult result;

    // Utility to monitor tempo drift
    DriftMonitor drifter;
    
    //
    // Things we pull from the AudioProcessor
    //

    double tempo = 0.0f;
    // whether the tempo was given to us by the host or derived from beat distance
    bool tempoSpecified = false;
    bool timeSignatureSpecified = false;
    int timeSignatureNumerator = 0;
    int timeSignatureDenominator = 0;
    bool playing = false;
    int hostBeat = -1;
    int hostBar = -1;

    //
    // Running Analysis
    //
    
    // this starts zero and increases on every block, used to timestamp things
    int audioStreamTime = 0;
    // used to derive beat widths and tempo
    int lastAudioStreamTime = 0;

    // the stream time of the last host beat
    int lastBeatTime = 0;

    // once tempo lock has been achieved the length of the base unit in samples
    // when this is zero, it means there is no tempo lock
    int unitLength = 0;

    // the location of a virtual playback position within the unit used
    // to generate normalized beats
    int unitPlayHead = 0;

    // total number of units that have elapsed since the start point
    int elapsedUnits = 0;
    // counter when unitsPerBeat is greater than 1
    int unitCounter = 0;
    
    // don't need this to be more than one, but might be interesting someday
    int unitsPerBeat = 1;

    int elapsedBeats = 0;

    // tempo monitoring
    double lastPpq = 0.0f;
    
    // Trace options
    bool traceppq = true;
    double lastppq = -1.0f;
    bool traceppqFine = false;
    int ppqCount = 0;

    void detectStart(bool newPlaying, double beatPosition);

    void ponderTempo(double newTempo);
    int tempoToUnit(double newTempo);
    void setUnitLength(int newLength);
    void resetTempoMonitor();
    void traceFloat(const char* format, double value);

    void ponderPpq(double beatPosition, int blockSize);
    double getBeatsPerSample(double currentPpq, int currentBlockSize);
    void deriveTempo(double samplesPerBeat);
    void checkUnitMath(int tempoUnit, double samplesPerBeat);
    
    void advanceAudioStream(int blockFrames);

};
