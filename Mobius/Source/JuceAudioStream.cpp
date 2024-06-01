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

// temporary 
#include "midi/MidiEvent.h"

#include "Supervisor.h"

#include "JuceAudioStream.h"

JuceAudioStream::JuceAudioStream(Supervisor* s)
{
    supervisor = s;
}

JuceAudioStream::~JuceAudioStream()
{
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
 */
int JuceAudioStream::getSampleRate()
{
    return (int)preparedSampleRate;
}

/**
 * When I used PortAudio, it had some services for calculating the effective
 * input and output latencies in a rather obscure way.  You wouldn't set a block
 * size you would "suggest" a latency and it would pick a block size, then you
 * had to call back to get the real latency.
 *
 * Now this should just be the block size.  
 * Still need work on this...
 */
int JuceAudioStream::getInputLatency()
{
    Trace(1, "JuceAudioStream::getInputLatencyFrames doesn't want you to know!\n");
    return 0;
}

int JuceAudioStream::getOutputLatency()
{
    Trace(1, "JuceAudioStream::getOutputLatencyFrames doesn't want you to know!\n");
    return 0;
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

/**
 * Return the object containing an analysis of the host transport time.
 * This will have been derived from AudioPlayHead by captureAudioTime
 * at the beginning of each audio block.
 */
AudioTime* JuceAudioStream::getAudioTime()
{
    AudioTime* at = nullptr;
    // only return this if we're a plugin and it has something to say
    if (supervisor->isPlugin())
      at = &audioTime;
    return at;
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

// hey, this was taken out of MobiusAudioStream and moved
// to MobiusContainer, so we don't need this here any more

void JuceAudioStream::midiSend(MidiEvent* msg)
{
    MidiManager* mm = supervisor->getMidiManager();
    
    juce::MidiMessage jmsg = juce::MidiMessage::noteOn(msg->getChannel(), msg->getKey(),
                                                       (juce::uint8)msg->getVelocity());
    mm->send(jmsg);
}

/**
 * This object provides Synchronizer with queued realtime events
 * and services for generating MIDI clocks.
 */
MobiusMidiTransport* JuceAudioStream::getMidiTransport()
{
    return supervisor->getMidiRealizer();
}

//////////////////////////////////////////////////////////////////////
//
// Audio Block Processing
//
// Now we get to the important bits.  PortAudio arranged things in
// interleaved "frame" buffers with each frame being a pair of stereo
// samples.  That is too hard to unwind so we continue that tradition
// and convert the Juce buffer styles into that format.
//
// When an audio blocks comes in the listener is notified with
// the processAudioStream() callback and passed JuceAudioStream
// which implements MobiusAudioStream.
//
// The listener is expected to immediately call getInterruptFrames
// and getInterruptBuffers on the stream to obtain the size of the block and the
// interleaved frame buffers for the ports it uses.
//
// Once we start handling plugin MIDI events it will call back to get
// those too.
//
// For the initial Juce port this has been simplified to a single input
// and output port and the interleaved buffers will have been constructred
// at the start of the Juce interrupt (getNextAudioBlock or processBlock)
// and left in a member variable before the listener is notified.
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by the MobiusListener after being notified of an incomming
 * audio block to get the size of the block.
 */
long JuceAudioStream::getInterruptFrames()
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
    (void)inport;
    (void)outport;
    if (input != nullptr) *input = inputBuffer;
    if (output != nullptr) *output = outputBuffer;
}

//////////////////////////////////////////////////////////////////////
//
// Standalone AudioAppComponent Interface
//
// Web chatter...
//
// There are no other guarantees. Some streams will call prepareToPlay()/releaseResources()
// repeatedly if they stop and start, others will not. It is possible for
// releaseResources() to be called when prepareToPlay() has not.
// Or another way to look at it that I find a little more intuitive:
// An AudioStream can only have two states, prepared and released,
// An AudioStream must always start and end its life in the released state…
// …but getNextAudioBlock() can only be called if the AudioStream is in the prepared state.
// prepareToPlay() puts the AudioStream into the prepared state;
// releaseResources() puts it in the released state.
// prepareToPlay() and releaseResources() can be called at any time, in any order.
//
// ---
// Prepare lets you know that the audio pipeline is about to start (or you’re about to
// be included in said pipeline if you’re a plugin), this is where you can create buffers or
// threads to use on the audio thread, and, if you’re a plugin this is where you will
// find out what SampleRate and BlockSize you’re going to be working with.
//
//////////////////////////////////////////////////////////////////////

/**
 * This function will be called when the audio device is started, or when
 * its settings (i.e. sample rate, block size, etc) are changed.
 *
 * The prepareToPlay() method is guaranteed to be called at least once on an 'unprepared'
 * source to put it into a 'prepared' state before any calls will be made to
 * getNextAudioBlock(). This callback allows the source to initialise any resources it
 * might need when playing.
 * 
 * Once playback has finished, the releaseResources() method is called to put the stream
 * back into an 'unprepared' state.
 *
 * Note that this method could be called more than once in succession without a matching
 * call to releaseResources(), so make sure your code is robust and can handle that kind
 * of situation.
 *
 * samplesPerBlockExpected
 *   the number of samples that the source will be expected to supply each time
 *   its getNextAudioBlock() method is called. This number may vary slightly, because it
 *   will be dependent on audio hardware callbacks, and these aren't guaranteed to always
 *   use a constant block size, so the source should be able to cope with small variations.
 *
 * sampleRate
 *  the sample rate that the output will be used at - this is needed by sources such
 *  as tone generators.
 */
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
}

/**
 * I think this should only be called during shutdown or after device configuration
 * changes.  For Mobius, it probably needs to perform a GlobalReset because
 * any synchronization calculations it was making may be wrong if the sample rate
 * changes on the next prepareToPlay call.
 */
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
    
    // do NOT call clearActiveBufferRegion like the boilerplate code
    // from Projucer, it is unclear whether this just does output buffers
    // or if it also wipes the input buffers too
    // the output buffers will get propery initialized during deinterleaving

    // number of samples we're expected to consume and fill
    // save this for the handler callback
    nextBlockSamples = bufferToFill.numSamples;
    
    // leave the result in our inputBuffer array
    interleaveInputBuffers(bufferToFill, inputBuffer);
    
    // clear the interleaved output buffer just in case the handler is bad
    clearInterleavedBuffer(nextBlockSamples, outputBuffer);

    // call the handler which will immediately call back to 
    // getInterruptFrames and getInterruptBuffers
    if (audioListener != nullptr) {
        audioListener->processAudioStream(this);
    }
    else {
        // inject a temporary test
        test(bufferToFill.numSamples);
    }

    // in case the listener didn't consume queued realtime message, flush the queue
    supervisor->getMidiRealizer()->flushEvents();
    
    // copy what was left in the in the interleaved output buffer back to
    // the Juce buffer, if we don't have a handler we'll copy the
    // empty output buffer left by clearInterleavedBuffer and clear
    // the channel buffers we didn't use
    deinterleaveOutputBuffers(bufferToFill, outputBuffer);

    // pray
}

//////////////////////////////////////////////////////////////////////
//
// AudioAppComponent Buffer Conversion
//
//////////////////////////////////////////////////////////////////////

/**
 * Wipe one of our interleaved buffers of content.
 * Only for the output buffer
 * Note that numSamples here is number of samples in one non-interleaved buffer
 * passed to getNextAudioBlock.  We need to multiply that by the number of channels
 * for the interleaved buffer.
 *
 * Our internal buffer will actually be larger than this (up to 4096 frames) but
 * just do what we need.
 */
void JuceAudioStream::clearInterleavedBuffer(int numSamples, float* buffer)
{
    // is it still fashionable to use memset?
    int totalSamples = numSamples * 2;
    memset(buffer, 0, (sizeof(float) * totalSamples));
}

/**
 * Convert a set of non-interleaved input buffers into an interleaved buffer.
 * Here we expect two channel buffers.  In theory there could be more but we'll ignore
 * them.  If there is only one, clear those samples so the engine doesn't
 * have to deal with it.
 *
 * The tutorial is kind of insane about how buffers are organized, so I'm capturing some
 * of it here:
 * 
 * It is important to know that the input and output buffers are not completely separate.
 * The same buffer is used for the input and output. You can test this by temporarily
 * commenting out all of the code in the getNextAudioBlock() function. If you then run
 * the application, the audio input will be passed directly to the output.
 * In the getNextAudioBlock() function, the number of channels in the AudioSampleBuffer
 * object within the bufferToFill struct may be larger than the number input channels,
 * the number of output channels, or both. It is important to access only the data that
 * refers to the number of input and output channels that you have requested, and that
 * are available. In particular, if you have more input channels than output channels you
 * must not modify the channels that should contain read-only data.
 *
 * Yeah, that sounds scary.
 *
 * In the getNextAudioBlock() function we obtain BigInteger objects that represent the list
 * of active input and output channels as a bitmask (this is similar to the std::bitset
 * class or using a std::vector<bool> object). In these BigInteger objects the channels
 * are represented by a 0 (inactive) or 1 (active) in the bits comprising the BigInteger value.
 *
 * Oh, do go on...
 *
 * To work out the maximum number of channels over which we need to iterate, we can inspect
 * the bits in the BigInteger objects to find the highest numbered bit. The maximum number
 * of channels will be one more than this.
 *
 * The code should be reasonably self-explanatory but here are a few highlights:
 *
 * [narrator: no, no it is not]
 *
 * [1]: We may have requested more output channels than input channels, so our app needs to
 * make a decision about what to do about these extra outputs. In this example we simply
 * repeat the input channels if we have more outputs than inputs. (To do this we use the
 * modulo operator to "wrap" our access to the input channels based on the number of input
 * channels available.) In other applications it may be more appropriate to output silence
 * for higher numbered channels where there are more output channels than input channels.
 * [2]: Individual input channels may be inactive so we output silence in this case too.
 *
 * Okay, let's see if I undersand this
 *
 * The getActiveFooChannels returns a big int like this
 *    0000101001
 * where each bit represents a channel and it will be set if it is active.  You
 * should only read from or write to channel buffers that are marked active.  In order
 * to properly silence output buffers you should iterate over all of them.  For input buffers
 * I guess you only need to consume what you need.  There can be a different number
 * of input and output channels.  To know how many you're allowed to iterate over
 * find the highest bit set.  Channels and bit numbers are zero based.
 * There is no guarantee that a pair of channels that are considered "stereo" will
 * be next to each other than that is simply a convention followed by the person plugging
 * things in to the hardware and enabling channels in the device manager.
 *
 * In most "normal" usage you plug each stereo channel into adjacent hardware jacks so
 * you should usually see this:
 *      000000011
 *
 * If they have chosen not to do that, we have to decide:
 *   1) use adjacent channels anyway and just consume/send silence if it is inactive
 *   2) arbitrarily find two active channels and merge them
 *
 * When you look at the AudioDeviceSelector component, you can pick different input
 * output devices.  There can be only one of each, unclear whether this is a Juce enforcement
 * or an OS enforcement.  You can also only use one "device type" at a time which appear
 * to be the device driver types: Windows Audio, ASIO, etc.
 *
 * In my case I'm using the Fireface.  For output selection the RME driver lists
 * a number of "devices" that represent physical ports arranged in stereo pairs.
 * "Speakers (RME Fireface UC)", "Analog 3+4", etc.  When you select the RME for either
 * input or output in the Active channels sections there is only one item "Output channel 1+2
 * and "Input channel 1+2".  I think because I asked the UI to group them, if left mono
 * there would be two items you could enable individually.  In theory other drivers might
 * provide larger clusters than two, pretty sure that's how it used to work, you just
 * selected "RME" and you got 8 or however many physical channels there were.  Maybe that's
 * a function of ASIO4All or maybe there is an RME driver setting to tweak that.
 *
 * In any case the driver gives you a list of devices to pick, and each device can have a
 * number of channels, and each channel can be selectively active or inactive.
 *
 * For the purposes of standalone Mobius, since we are in control over the device
 * configuration we can say you must activate a pair of channels for stereo and it
 * is recommended that they be adjacent.  But we don't have to depend on that, if you
 * wanted L going into jack 1 and R going into jack 5 who are we to judge.  But we're not
 * going to provide a UI that lets to pick which channels should be considered a stereo
 * pair.  This is enforced to a degree by making the selector UI group them in pairs, but
 * in cases where there is an odd number you still might end up with a "pair" that has only one
 * channel, I think.  Maybe it just doesn't show those.
 *
 * To allow for configuration flexibility, we'll take option 2 above.  Find the lowest numbered
 * pair of active channels and combine them into our stereo buffer.  If someone wanted
 * to say put all the Left channels on jacks 1-4 and all the Right channels on jacks
 * 5-8 they couldn't do that, but then they have larger problems in life.
 *
 * Anyway, this is where we could implement the old concept of "ports".  Find as many
 * pairs of active channels as you can and treat those as ports.  However, the RME driver
 * only shows 2 channels per "device" so we can't do that without changing something
 * in the driver.
 *
 * I picked the wrong week to give up drinking...
 *
 * Sweet jesus, bufferToFill.buffer->getReadPointer does not return a float*
 * it is templated and returns "something" you apparently aren't suposed to know
 * and the examples hide this by using "auto" variables to hide the type.  It may be float*
 * or I guess it may be double*.  The compiler complains if you try to assign it to a float*
 * with "C2440	'=': cannot convert from 'const Type *' to 'float *"
 * This means you can't easily iterate over them and save the pointers for later.
 * Instead we'll iterate and remember just the channel numbers, then call getReadPointer
 * in the next loop where we need to access the samples.  Fuck.
 */
void JuceAudioStream::interleaveInputBuffers(const juce::AudioSourceChannelInfo& bufferToFill,
                                                float* resultBuffer)
{
    // to do the active channel folderol, we need to get to the AudioDeviceManager
    juce::AudioDeviceManager* deviceManager = Supervisor::Instance->getAudioDeviceManager();
    auto* device = deviceManager->getCurrentAudioDevice();
    auto activeInputChannels = device->getActiveInputChannels();
    auto maxInputChannels = activeInputChannels.getHighestBit() + 1;

    // because we can't be sure we'll find 2 or any active channels, zero
    // out the result buffer so we don't have to clean up the mess after
    // the next loop
    clearInterleavedBuffer(bufferToFill.numSamples, resultBuffer);
    
    // look for the two lowest numbered active channels to serve as the source
    // for our stereo frames, when we find one, copy them as soon as we find them
    // so we can use the fucking auto
    int interleavedChannel = 0;
    for (auto channel = 0 ; channel < maxInputChannels ; channel++) {
        // this is a misnomer, it isn't the channels that are active
        // it's a bit vector of channels that could be active
        if (activeInputChannels[channel]) {
            // okay, here's one
            auto* buffer = bufferToFill.buffer->getReadPointer(channel, bufferToFill.startSample);
            for (int i = 0 ; i < bufferToFill.numSamples ; i++) {
                int frameOffset = i * 2;
                resultBuffer[frameOffset + interleavedChannel] = buffer[i];
            }
            interleavedChannel++;
            if (interleavedChannel >= 2)
              break;
        }
    }

    // our job was just to get the input samples, output comes later
    // after the listener is called
}

/**
 * Take the interleaved Mobius output buffer and spray that into active
 * Juce output channel buffers.
 *
 * Similar issues with output channels that represent stereo pairs.
 * In practice these will almost always be the first two but if not pick
 * the lowest two we can find.
 *
 * For any active output channels that we decide not to use, fill them
 * with zeros.  I think that is required because they're not cleared
 * by default.  Or we could put a jaunty tune in there.
 */
void JuceAudioStream::deinterleaveOutputBuffers(const juce::AudioSourceChannelInfo& bufferToFill,
                                                   float* sourceBuffer)
{
    // to do the active channel folderol, we need to get to the AudioDeviceManager
    // once this is working, can save some of this so we don't have to do it all again
    // but it should be fast enough
    juce::AudioDeviceManager* deviceManager = Supervisor::Instance->getAudioDeviceManager();
    auto* device = deviceManager->getCurrentAudioDevice();
    auto activeOutputChannels = device->getActiveOutputChannels();
    auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;
    
    // first locate the two lowest numbered active channels to serve as the
    // destination for our stereo frames
    int interleavedChannel = 0;
    for (int channel = 0 ; channel < maxOutputChannels ; channel++) {
        if (activeOutputChannels[channel]) {
            auto* buffer = bufferToFill.buffer->getWritePointer(channel, bufferToFill.startSample);
            if (interleavedChannel < 2) {
                for (int i = 0 ; i < bufferToFill.numSamples ; i++) {
                    int frameOffset = i * 2;
                    buffer[i] = sourceBuffer[frameOffset + interleavedChannel];
                }
            }
            else {
                // an extra one, clear it
                for (int i = 0 ; i < bufferToFill.numSamples ; i++) {
                    buffer[i] = 0.0f;
                }
            }
            interleavedChannel++;
        }
    }
    
    // if you're like me you're tired, but it's a good kind of tired
}

//////////////////////////////////////////////////////////////////////
//
// Plugin AudioProcessor Interface
//
// These are conceptually similar to the callback used in an
// AudioAppCompoenent but the signatures are different.
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

/**
 * Now the hard part.
 * Like getNextAudioBlock in standalone applciations we need to convert
 * the AudioBuffer style into interleaved buffers.
 *
 * Since I kind of know what's going on now, there is less diagnostic trace
 * in this one than there is in getNextAudioBlock.
 *
 * Everything is port zero, first two channels again, but we need fix that asap.
 * 
 * This time we get the number of samples to consume/fill (aka the block size)
 * from buffer.getNumSamples where getNextAudioBlock got it from AudioSourceChannelInfo.
 * We don't have to mess with "startSamples", it's always zero to the block size.
 *
 * Using "auto" again since the buffer can contain either float or double and it
 * whines about casting.
 */
void JuceAudioStream::processBlockPlugin(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    processBlockCalls++;

    captureAudioTime(buffer.getNumSamples());
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
        // example shows going back here to get the number of channels
        // every time, it's less complicated than standalone where we have that
        // freaking bit vector to deal with
        // not that it matters, but can we cache this in prepareToPlay?
        juce::AudioProcessor* processor = supervisor->getAudioProcessor();
        // wtf does this do?
        // oh, this is very interesting, explore this for test file comparison...
        juce::ScopedNoDenormals noDenormals;
        auto inputChannels  = processor->getTotalNumInputChannels();
        auto outputChannels = processor->getTotalNumOutputChannels();
        int blockSize = buffer.getNumSamples();
        
        interleaveInputBuffers(buffer, 0, inputChannels, blockSize, inputBuffer);

        // clear the interleaved output buffer just in case the handler is bad
        clearInterleavedBuffer(blockSize, outputBuffer);

        // this is what the listener will ask for
        nextBlockSamples = blockSize;
    
        // call the handler which will immediately call back to 
        // getInterruptFrames and getInterruptBuffers
        // and now getMidiMessages
        if (audioListener != nullptr) {
            nextMidiMessages = &midiMessages;
            audioListener->processAudioStream(this);
        }

        // copy what was left in the in the interleaved output buffer back to
        // the Juce buffer, if we don't have a handler we'll copy the
        // empty output buffer left by clearInterleavedBuffer and clear
        // the channel buffers we didn't use
        deinterleaveOutputBuffers(buffer, 0, outputChannels, blockSize, outputBuffer);

        // pray
    }

    // do what Projucer did until we can figure this out
    //processBlockDefault(buffer, midiMessages);
}

/**
 * Interleave two adjacent input channels into the temporary buffer.
 */
void JuceAudioStream::interleaveInputBuffers(juce::AudioBuffer<float>& buffer, int port, int maxInputs,
                                                 int blockSize, float* resultBuffer)
{
    int channelOffset = port * 2;

    if (channelOffset >= maxInputs) {
        // don't have at least one channel, must have a misconfigured port number
        clearInterleavedBuffer(blockSize, resultBuffer);
    }
    else {
        // should have 2 but if there is only one go mono
        auto* channel1 = buffer.getReadPointer(channelOffset);
        auto* channel2 = channel1;
        if (channelOffset + 1 < maxInputs)
          channel2 = buffer.getReadPointer(channelOffset + 1);

        // !! do some range checking on the result buffer
        int sampleIndex = 0;
        for (int i = 0 ; i < blockSize ; i++) {
            resultBuffer[sampleIndex] = channel1[i];
            resultBuffer[sampleIndex+1] = channel2[i];
            sampleIndex += 2;
        }
    }
}

/**
 * Deinterleave a port output buffer into two adjacent Juce buffers.
 * We've got to fill all Juce buffers with something because they may start
 * with garbage.
 */
void JuceAudioStream::deinterleaveOutputBuffers(juce::AudioBuffer<float>& buffer, int port,
                                                    int maxOutputs, int blockSize, float* sourceBuffer)
{
    int destChannelOffset = port * 2;

    for (int channel = 0 ; channel < maxOutputs ; channel++) {
        auto* destSamples = buffer.getWritePointer(channel);

        float* srcSamples = nullptr;
        if (channel == destChannelOffset) {
            // this one wants to receive the left channel
            srcSamples = sourceBuffer;
        }
        else if (channel == (destChannelOffset + 1)) {
            // this one wants to receive the right channel
            srcSamples = sourceBuffer + 1;
        }
        else {
            // this one is out of range, leave null
        }

        // now that we've determined the source pointer, copy them
        for (int i = 0 ; i < blockSize ; i++) {
            if (srcSamples == nullptr) {
                destSamples[i] = 0.0f;
            }
            else {
                destSamples[i] = *srcSamples;
                // skip to the next frame
                srcSamples += 2;
            }
        }
    }
    
    // if you're like me you're tired, but it's a good kind of tired
}

/**
 * This is what the generated Projecer code did, keep it around for ahwile
 * as a reference.
 */
void JuceAudioStream::processBlockDefault(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    (void)midiMessages;
    // getTotal... methods are defined on this so we have to go back there
    // since we're not in Kansas anymore
    juce::AudioProcessor* processor = supervisor->getAudioProcessor();
    
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = processor->getTotalNumInputChannels();
    auto totalNumOutputChannels = processor->getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        //auto* channelData = buffer.getWritePointer (channel);

        // ..do something to the data...
    }
}

//////////////////////////////////////////////////////////////////////
//
// Tests
//
//////////////////////////////////////////////////////////////////////

/**
 * If we don't have a handler we'll call down here and inject
 * something interesting for testing.
 *
 * Start by just copying the input to the output to verify
 * that interleaving works.
 */
void JuceAudioStream::test(int numSamples)
{
    // input buffer has been filled, copy them to the output buffer buffer

    int frameSamples = numSamples * 2;
    for (int i = 0 ; i < frameSamples ; i++) {
        outputBuffer[i] = inputBuffer[i];
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

/**
 * Called at the beginning of each audio block to convert state
 * from the AudioPlayHead into the AudioTime model used
 * by Synchronizer.
 *
 * See HostSyncState.cpp for more on the history of this.
 *
 * We start by feeding the state from the AudioHead through the
 * HostSyncState object which does analysis.  The results of that
 * analysis are then deposited in an AudioTime that will be returned
 * by MobiusAudioStream::getAudioTime when the Synchronizer is being used.
 */
void JuceAudioStream::captureAudioTime(int blockSize)
{
    juce::AudioProcessor* processor = supervisor->getAudioProcessor();
    juce::AudioPlayHead* head = processor->getPlayHead();

    if (head != nullptr) {
        juce::Optional<juce::AudioPlayHead::PositionInfo> pos = head->getPosition();
        if (pos.hasValue()) {

            bool isPlaying = pos->getIsPlaying();
            
            // haven't cared about getIsLooping in the past but that might be
            // interesting to explore

            // updateTempo needs: sampleRate, tempo, numerator, denominator
            
            // sample rate is expected to be an int, Juce gives
            // us a double, under what conditions would this be fractional?
            int sampleRate = (int)preparedSampleRate;

            double tempo = 0.0;
            juce::Optional<double> bpm = pos->getBpm();
            if (bpm.hasValue()) tempo = *bpm;
            
            int tsigNumerator = 0;
            int tsigDenominator = 0;
            juce::Optional<juce::AudioPlayHead::TimeSignature> tsig = pos->getTimeSignature();
            if (tsig.hasValue()) {
                tsigNumerator = tsig->numerator;
                tsigDenominator = tsig->denominator;
            }

            syncState.updateTempo(sampleRate, tempo, tsigNumerator, tsigDenominator);

            // advance needs: frames, samplePosition, beatPosition,
            // transportChanged, transportPlaying
            
            // transportChanged is an artifact of kVstTransportChanged in VST2
            // and CallHostTransportState in AUv1 that don't seem to exist in
            // VST3 and AU3
            // so we can keep using the old HostSyncState code, derive transportChanged just by
            // comparing isPlaying to the last value
            bool transportChanged = isPlaying != syncState.isPlaying();

            // samplePosition was only used for transport detection in old hosts
            // that didn't support kVstTransportChanged, shouldn't need that any more
            // but we can pass it anyway.  For reasons I don't remember samplePosition
            // was a double, but getTimeInSamples is an int, shouldn't matter
            double samplePosition = 0.0;
            juce::Optional<int64_t> tis = pos->getTimeInSamples();
            if (tis.hasValue()) samplePosition = (double)(*tis);
            
            // beatPosition is what is called "ppq position" everywhere else
            double beatPosition = 0.0;
            juce::Optional<double> ppq = pos->getPpqPosition();
            if (ppq.hasValue()) beatPosition = *ppq;

            // HostSyncState never tried to use "bar" information from the host
            // because it was so unreliable as to be useless, things may have
            // changed by now.  It will try to figure that out it's own self,
            // would be good to verify that it matches what Juce says...
            
            syncState.advance(blockSize, samplePosition, beatPosition,
                              transportChanged, isPlaying);

            // now dump that mess into an AudioTime for Mobius
            syncState.transfer(&audioTime);
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
