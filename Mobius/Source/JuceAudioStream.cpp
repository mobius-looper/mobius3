/**
 * An implementation of MobiusAudioStream that bridges Juce audio
 * buffer conventions to the old world.
 *
 * As interrupts come in, we need to convert the Juce buffer structure
 * with separate channel arrays into an interleavad array and save them.
 * Then call the listener, which will immediately calls us back and ask for those
 * buffers.  The "ports" concept was to support more than 2 channels.  Say the hardware
 * had 8 channels.  These would be presented as 4 ports with 2 channels each, left and right.
 * In theory you could have more than 2 channels per port for surround but that was
 * never implemented.
 *
 * For the initial integration, we'll assume 2 stereo channels per port and only one port.
 * This is all the RME seems to allow anyway.  Leave multiple ports for another day.
 *
 * Besides buffers, the stream is expected to provide the the sample rate for synchronization.
 * We save that at startup in the Juce prepareToPay call, it can presumably change
 * if you reconfigure the hardware, I believe Juce is supposed to call releasesResources
 * and prepareToPlay with the new rate when that happens, but there is some ambiguity.
 *
 * There are two sets of callbacks in here, one when running standalone under an AudioAppComponent
 * and one when running as a plugin under AudioProcessor.
 *
 * The stream will be given to each of those two objects which will then start forwarding
 * the Juce callbacks to the stream.
 *
 * The stream has a Listener which will receive notifications whenever a new block is ready.
 * In practice this will always be MobiusKernel.  So the flow of control is:
 *
 *     standalone:
 *       MainComponent->Supervisor->JuceAudioStream->MobiusAudioListener->MobiusKernel
 *
 *     plugin:
 *
 *       AudioProcessor->Supervisor->JuceAudioStream->MobiusAudioListener->MobiusKernel
 *
 * There is a lot of commentary in here from when I was taking notes trying to understand
 * how things worked.  That can be rewritten and moved to an external notes file.
 *
 * There is a lot of audio callback state tracking that I added for testing to try
 * to understand what was being called and when that isn't necessary any more.
 *
 */

#include <JuceHeader.h>

#include "util/Trace.h"

#include "mobius/MobiusInterface.h"

#include "Supervisor.h"

#include "JuceAudioStream.h"

JuceAudioStream::JuceAudioStream(Supervisor* s)
{
    supervisor = s;
}

JuceAudioStream::~JuceAudioStream()
{
}

void JuceAudioStream::configure()
{
    portAuthority.configure(supervisor);
}

void JuceAudioStream::traceFinalStatistics()
{
    Tracej("AudioStream: Ending audio callback statistics:");
    Tracej("  prepareToPlay " + juce::String(prepareToPlayCalls));
    Tracej("  getNextAudioBlock " + juce::String(getNextAudioBlockCalls));
    Tracej("  releaseResources " + juce::String(releaseResourcesCalls));
    if (audioPrepared) 
      Tracej("  Ending with audio still prepared!");
}

/**
 * Register the listener to receive notifications as
 * audio buffers come in.  This will always be a MobiusKernel.
 */
void JuceAudioStream::setAudioListener(MobiusAudioListener* l)
{
    audioListener = l;
}

//////////////////////////////////////////////////////////////////////
//
// Stream Properties
//
//////////////////////////////////////////////////////////////////////

/**
 * Some parts of the synchronization system need to know the sample rate
 * in order to convert wall clock time to audio stream time.
 *
 * Juce gives us a float but I've always used this as an int.
 * Under what circumstances would this have a fraction?
 */
int JuceAudioStream::getSampleRate()
{
    return (int)preparedSampleRate;
}

/**
 * The stream block size becomes the default input and output latency for
 * compensation in core code.  This is often overridden in MobiusConfig.
 */
int JuceAudioStream::getBlockSize()
{
    return (int)preparedSamplesPerBlock;
}

/**
 * I don't remember exactly what this was.
 * getAudioTime is the important one, look for uses of this and get rid
 * of them if we can.
 */
double JuceAudioStream::getStreamTime()
{
    return 0.0;
}

double JuceAudioStream::getLastInterruptStreamTime()
{
    return 0.0;
}

//////////////////////////////////////////////////////////////////////
//
// MIDI
//
//////////////////////////////////////////////////////////////////////

/**
 * Return the MidiBuffer we received on the last processBlockPlugin
 * Since this started as a reference, it really would be better if we just
 * passed this to processAudioStream and skipped the callback style
 * for these.
 */
juce::MidiBuffer* JuceAudioStream::getMidiMessages()
{
    return nextMidiMessages;
}

//////////////////////////////////////////////////////////////////////
//
// Audio Block Processing
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by the MobiusListener after being notified of an incomming
 * audio block to get the size of the block.
 */
int JuceAudioStream::getInterruptFrames()
{
    return nextBlockSamples;
}

/**
 * Called by the MobiusListener after being notified of an incomming
 * audio block to get the interleaved frame buffers for one of
 * the input and output ports.  Each track will call this since
 * tracks can have different port configurations.
 *
 * Since we have simplified this to a single pair of IO ports,
 * we just return the buffers converted at the start of the interrupt.
 *
 * Note that when the noExternalInput test parameter is on,
 * this will be called with nullptr for the output buffer
 * since it is not needed.  But check both to be safe.
 */
void JuceAudioStream::getInterruptBuffers(int inport, float** input, 
                                              int outport, float** output)
{
    if (input != nullptr) *input = portAuthority.getInput(inport);
    if (output != nullptr) *output = portAuthority.getOutput(outport);
}

//////////////////////////////////////////////////////////////////////
//
// Standalone AudioAppComponent Interface
//
//////////////////////////////////////////////////////////////////////

void JuceAudioStream::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    if (prepareToPlayCalls == 0) {
        // first time here, trace to something to understand when things start
        // happening during initialization
        Tracej("AudioStream: prepareToPlay first call");
    }
    else if (audioPrepared) {
        // called again in an already prepared state without calling releaseResources
        // can this happen?
        Tracej("AudioStream: prepareToPlay already prepared");
        if (samplesPerBlockExpected != preparedSamplesPerBlock)
          Tracej("  samplesPerBlock changing from " + juce::String(preparedSamplesPerBlock));
        if (sampleRate != preparedSampleRate)
          Tracej("  sampleRate changing from " + juce::String(preparedSampleRate));
    }
    else {
        Tracej("AudioStream: prepareToPlay");
    }
    Tracej("  samplesPerBlock " + juce::String(samplesPerBlockExpected));
    Tracej("  sampleRate " + juce::String(sampleRate));

    // wasn't set up during construction, better be by now
    //supervisor->traceDeviceSetup();

    prepareToPlayCalls++;
    preparedSamplesPerBlock = samplesPerBlockExpected;
    preparedSampleRate = sampleRate;
    
    audioPrepared = true;

    // after every prepareToPlay, trace the configuration of the next AudioBuffer
    blocksAnalyzed = false;
}

void JuceAudioStream::releaseResources()
{
    releaseResourcesCalls++;

    Trace("AudioStream: releaseResources");
    
    audioPrepared = false;
}

/**
 * Outer block handler that keeps track of a bunch of things I
 * wanted to know during porting.  Eventually makes its way
 * to getNextAudioBlockForReal()
 * 
 * AudioSourceChannelInfo is a simple struct with attributes
 *    AudioBuffer<float>* buffer
 *    int startSample
 *      the first sample in the buffer from which the callback is expected to write data
 *    int numSamples
 *      the number of samples in the buffer which the callback is expected to fill with data
 *
 * AudioBuffer
 *    packages the buffer arrays and various sizing info
 */
void JuceAudioStream::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    getNextAudioBlockCalls++;

    // temporary diagnostics for the configuration of the AudioBuffer under
    // various port/channel activation scenarios
    if (!blocksAnalyzed) {
        Trace(2, "JuceAudioStream: Analyzing AudioBuffer configuration\n");
        juce::AudioBuffer<float>* buffer = bufferToFill.buffer;
        int channels = buffer->getNumChannels();

        Trace(2, "  %d channels\n", channels);

        juce::AudioDeviceManager* deviceManager = supervisor->getAudioDeviceManager();
        auto* device = deviceManager->getCurrentAudioDevice();
        auto activeInputChannels = device->getActiveInputChannels();
        Trace(2, "  Active input channels %s\n", activeInputChannels.toString(2).toUTF8());
        
        auto activeOutputChannels = device->getActiveOutputChannels();
        Trace(2, "  Active output channels %s\n", activeOutputChannels.toString(2).toUTF8());

        // so technically we're supposed to ignore the channel buffers for the
        // channels that don't have the active bit set in those two bit vectors
        
        // I would expect all of these to be non-null since they are bi-directional
        // unless getReadPointer and getWritePointer return different things
        int nulls = 0;
        for (int i = 0 ; i < channels ; i++) {
            auto* samples = buffer->getReadPointer(i, bufferToFill.startSample);
            if (samples == nullptr)
              nulls++;
        }
        if (nulls > 0)
          Trace(2, "  %d null read buffers encountered\n", nulls);

        nulls = 0;
        for (int i = 0 ; i < channels ; i++) {
            auto* samples = buffer->getWritePointer(i, bufferToFill.startSample);
            if (samples == nullptr)
              nulls++;
        }
        if (nulls > 0)
          Trace(2, "  %d null write buffers encountered\n", nulls);

        blocksAnalyzed = true;
    }

    // this clears everything we are expected to write to
    // until the engine is fully functional, start with this to avoid noise
    // NO: unclear whether this just clears the output buffers or the inputs
    // too, need to read more, till then let the test scaffolding handle it
    //bufferToFill.clearActiveBufferRegion();

    // outer object has a startSample and numSamples
    // the buffer can apparently be larger than the amount we're asked to fill
    // I'm interested in whether the buffer size is variable or if it always
    // stays at preparedSamplesPerBlock, and whether startSample jumps areound or
    // statys at zero
    if (audioLastSourceStartSample != bufferToFill.startSample) {
        Tracej("AudioStream: getNextAudioBlock noticing audio source start sample " + juce::String(bufferToFill.startSample));
        audioLastSourceStartSample = bufferToFill.startSample;
    }
    if (audioLastSourceNumSamples != bufferToFill.numSamples) {
        Tracej("AudioStream: getNextAudioBlock noticing audio source num samples " + juce::String(bufferToFill.numSamples));
        audioLastSourceNumSamples = bufferToFill.numSamples;
    }

    // number of channels, I think this should match MainComponent asked for in setAudioChannels
    // currently 4 input and 2 output
    // always getting 2 here, which means that this callback can't support
    // different numbers for input and output channels?  makes sense for this interface
    // but I think you can actually configure the hardware to have different numbers internally,
    // works well enough for now, I alwaysd used 8 for both
    int channels = bufferToFill.buffer->getNumChannels();
    if (audioLastBufferChannels != channels) {
        Tracej("AudioStream: getNextAudioBlock noticing audio buffer channels " + juce::String(channels));
        audioLastBufferChannels = channels;
    }

    // number of channels of audio data that this buffer contains
    // this may not match what the source wants us to fill?
    // clearActiveBufferRegion seems to imply that
    int samples = bufferToFill.buffer->getNumSamples();
    if (audioLastBufferSamples != samples) {
        Tracej("AudioStream: getNextAudioBlock noticing audio buffer samples " + juce::String(samples));
        audioLastBufferSamples = samples;
    }
    
    // can these ever be different
    if (samples > audioLastSourceNumSamples) {
        Tracej("AudioStream: getNextAudioBlock  buffer is larger than requested");
        // startSample should be > 0 then because we're only
        // filling a portion of the buffer
        // doesn't really matter, Juce may want to deal with larger
        // buffers for some reason but it raises latency questions
    }
    
    // you can call setSample and various gain utility methods
    // buffer.setSample(destChannel, int destSample, valueToAdd)
    // I don't think this is required, float* getWritePointer
    // just returns the raw array

    // buffer.getReadPointer(channel, sampleIndex)
    // returns a pointer to an array of read-only samples of one channel,
    // KEY POINT: Unlike PortAudio, the samples are not interleaved into "frames"
    // containing samples for all channels.  You will have to build that
    // AudioBuffer<Type> has a variable type, you can't assume it is AudioBuffer<float>
    // and seems to be usually AudioBuffer<double> exaples use "auto" to hide the difference
    if (!audioPrepared) {
        if (!audioUnpreparedBlocksTraced) {
            // this isn't supposed to happen, right?
            Tracej("AudioStream: getNextAudioBlock called in an unprepared state");
            // prevent this from happening more than once if it's common
            // audioUnpreparedBlocksTraced = true;
        }
        // until we know if this is a state that's supposed to happen
        // prevent passing this along to Mobius
    }
    else {
        getNextAudioBlockForReal(bufferToFill);
    }
}

/**
 * Do format conversion on the audio block data and notify the listener.
 */
void JuceAudioStream::getNextAudioBlockForReal (const juce::AudioSourceChannelInfo& bufferToFill)
{
#ifdef USE_FFMETERS    
    meterSource.measureBlock(*(bufferToFill.buffer));
#endif

    // number of samples we're expected to consume and fill
    // save this for the handler callback
    nextBlockSamples = bufferToFill.numSamples;
    
    portAuthority.prepare(bufferToFill);

    if (audioListener != nullptr)
      audioListener->processAudioStream(this);

    portAuthority.commit();
    
    // in case the listener didn't consume queued realtime message, flush the queue
    // !!! NO
    // I did this so that events wouldn't stack up when in the debugger and flood the
    // Synchronizer when continued, but this causes rapid events like clocks to get lost
    // if they came in during the processing of the last block.  Need another way to detect
    // a hung engine
    //supervisor->getMidiRealizer()->flushEvents();
}

//////////////////////////////////////////////////////////////////////
//
// Plugin AudioProcessor Interface
//
//////////////////////////////////////////////////////////////////////

void JuceAudioStream::prepareToPlayPlugin(double sampleRate, int samplesPerBlock)
{
    if (prepareToPlayCalls == 0) {
        // first time here, trace to something to understand when things start
        // happening during initialization
        Tracej("AudioStream: prepareToPlayPlugin first call");
    }
    else if (audioPrepared) {
        // called again in an already prepared state without calling releaseResources
        // can this happen?
        Tracej("AudioStream: prepareToPlayPlugin already prepared");
        if (sampleRate != preparedSampleRate)
          Tracej("  sampleRate changing from " + juce::String(preparedSampleRate));
        if (samplesPerBlock != preparedSamplesPerBlock)
          Tracej("  samplesPerBlock changing from " + juce::String(preparedSamplesPerBlock));
    }
    else {
        Tracej("AudioStream: prepareToPlayPlugin");
    }

    prepareToPlayCalls++;
    preparedSamplesPerBlock = samplesPerBlock;
    preparedSampleRate = sampleRate;
    
    Tracej("AudioStream: prepareToPlayPlugin samplesPerBlock " + juce::String(samplesPerBlock) +
          " sampleRate "  + juce::String(sampleRate));

    audioPrepared = true;
}

void JuceAudioStream::releaseResourcesPlugin()
{
    releaseResourcesCalls++;

    Trace("AudioStream: releaseResources");

    audioPrepared = false;
}

void JuceAudioStream::processBlockPlugin(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    processBlockCalls++;

    // moved to sync/HostAnalyzer
    //captureAudioTime(buffer.getNumSamples());
    //tracePlayHead();
    
    if (!audioPrepared) {
        if (!audioUnpreparedBlocksTraced) {
            // this isn't supposed to happen, right?
            Tracej("AudioStream: processBlock called in an unprepared state");
            // prevent this from happening more than once if it's common
            // audioUnpreparedBlocksTraced = true;
        }
        // until we know if this is a state that's supposed to happen
        // prevent passing this along to Mobius
    }
    else {
        // used in the tutorials
        // wtf does this do?
        // oh, this is very interesting, explore this for test file comparison...
        juce::ScopedNoDenormals noDenormals;

        // this is what the listener will ask for
        nextBlockSamples = buffer.getNumSamples();
        nextMidiMessages = &midiMessages;
    
        portAuthority.prepare(buffer);
        
        if (audioListener != nullptr)
          audioListener->processAudioStream(this);

        portAuthority.commit();
    }
}

//////////////////////////////////////////////////////////////////////
//
// AudioPlayHead and AudioTime
//
//////////////////////////////////////////////////////////////////////

/**
 * Try to figure out what's going on with AudioPlayHead
 * This is debug code not active normally, and can be deleted after
 * we've tested with a few hosts.
 */
static int TracePlayHeadCalls = 0;
static int TracePlayHeadEmits = 0;

void JuceAudioStream::tracePlayHead()
{
    juce::AudioProcessor* processor = supervisor->getAudioProcessor();
    juce::AudioPlayHead* head = processor->getPlayHead();
    if (head != nullptr) {
        if (TracePlayHeadEmits == 0) {
            Trace(2, "AudioPlayHead: first call after %d blocks\n", TracePlayHeadCalls);
            Trace(2, "  canControlTransport %s\n",
                  (head->canControlTransport() ? "true" : "false"));
        }

        juce::Optional<juce::AudioPlayHead::PositionInfo> pos = head->getPosition();
        if (pos.hasValue()) {

            if (TracePlayHeadEmits == 0) {
                Trace(2, "AudioPlayHead: PositionInfo available after %d blocks\n",
                      TracePlayHeadCalls);
            }

            if ((TracePlayHeadEmits % 100) == 0) {
                Trace(2, "AudioPlayHead:\n");
                Trace(2, "  isPlaying %d isRecording %d is Looping %d\n",
                      (int)pos->getIsPlaying(), (int)pos->getIsRecording(), (int)pos->getIsLooping());
            
                // Optional pain is not optional
            
                Trace(2, "  timeInSamples %d timeInSeconds %d hostTimeNs %d\n",
                      getOptional(pos->getTimeInSamples()), getOptional(pos->getTimeInSeconds()),
                      getOptional(pos->getHostTimeNs()));

                int tsigNumerator = 0;
                int tsigDenominator = 0;
                juce::Optional<juce::AudioPlayHead::TimeSignature> tsig = pos->getTimeSignature();
                if (tsig.hasValue()) {
                    tsigNumerator = tsig->numerator;
                    tsigDenominator = tsig->denominator;
                }
                Trace(2, "  bpm %d time signature %d/%d\n",
                      getOptional(pos->getBpm()), tsigNumerator, tsigDenominator);
                
                Trace(2, "  barCount %d ppqLastBar %d ppq %d\n",
                      getOptional(pos->getBarCount()), getOptional(pos->getPpqPositionOfLastBarStart()),
                      getOptional(pos->getPpqPosition()));

                // todo:
                // FrameRate, not interesting
                // LoopPoints, might be interesting someday
                // EditOriginTime, not sure what this means
            }
            TracePlayHeadEmits++;
        }
    }
    TracePlayHeadCalls++;
}

int JuceAudioStream::getOptional(juce::Optional<int64_t> thing)
{
    return (thing.hasValue() ? (int)(*thing) : -1);
}

int JuceAudioStream::getOptional(juce::Optional<uint64_t> thing)
{
    return (thing.hasValue() ? (int)(*thing) : -1);
}

int JuceAudioStream::getOptional(juce::Optional<double> thing)
{
    return (thing.hasValue() ? (int)(*thing * 100) : -1);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
