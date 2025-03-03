/**
 * Utilities for mapping between juce::AudioBuffer and the interleaved
 * port buffers used by core code.
 *
 */

#include <JuceHeader.h>

#include "model/old/MobiusConfig.h"
#include "model/DeviceConfig.h"
#include "Supervisor.h"
#include "PortAuthority.h"

//////////////////////////////////////////////////////////////////////
//
// PortAuthority
//
//////////////////////////////////////////////////////////////////////

PortAuthority::PortAuthority()
{
}

PortAuthority::~PortAuthority()
{
}

/**
 * Capture some things from the environment at the startup of the plugin.
 * Here we probably need to deal with the whole "Bus" thing which
 * looks insanely complicated, but I think I can skip that for now
 * because I don't have "side chains" or "aux busses".   Should explore
 * that though.
 */
void PortAuthority::configure(Supervisor* super)
{
    juce::AudioProcessor* processor = super->getAudioProcessor();
    
    if (processor != nullptr) {
        // we're a plugin
        isPlugin = true;
        
        // docs: This method will return the total number of input channels
        // by accumulating the number of channels on each input bus
        // the number of channels of the buffer passed to your processBlock
        // callback will be equivalent to either getTotalNumInputChannels or
        // getTotalNumOutputChannels - whichever is greater
        //
        // jsl - can this change at runtime, or is it fixed at startup?
        pluginInputChannels = processor->getTotalNumInputChannels();
        pluginOutputChannels = processor->getTotalNumOutputChannels();
    }
    else {
        // we're standalone, pass in what?
    }

    int maxPorts = 0;
    if (isPlugin) {
        // this was originally configurable as "pins" in MobiusConfig
        // we now do something similar in DeviceConfig/PluginConfig
        // unlike standalone, these can't be dynamically resized without
        // restarting the plugin so we don't necessarily need to pre-allocate
        // more than we'll need at runtime
        DeviceConfig* config = super->getDeviceConfig();
        juce::PluginHostType host;
        HostConfig* hostConfig = config->pluginConfig.getHostConfig(host.getHostDescription());
        int maxAux = 0;
        if (hostConfig == nullptr) {
            maxAux = config->pluginConfig.defaultAuxOutputs;
            if (config->pluginConfig.defaultAuxInputs > maxAux)
              maxAux = config->pluginConfig.defaultAuxInputs;
        }
        else {
            // !! todo: the model allows each io bus to have more than two
            // channels, which if configured would make the AudioBuffer have unusual
            // channel groupings that we don't understand
            // continue assuming everything has 2 channels, but need to revisit this
            maxAux = hostConfig->inputs.size();
            if (hostConfig->outputs.size() > maxAux)
              maxAux = hostConfig->outputs.size();
        }
        
        // always the single main bus
        maxPorts = maxAux + 1;
    }
    else {
        // this was always harded to 16 in old Mobius
        // in the new, it should be in devices.xml
        // it is less important now since we could just dynamically
        // adapt to the number of channels in the AudioBuffer and grow
        // as necessary
        maxPorts = 16;
    }
    
    for (int i = 0 ; i < maxPorts ; i++) {
        PortBuffer* pb = new PortBuffer();
        ports.add(pb);
    }
}

/**
 * Get ready to serve buffers when running as a standalone application.
 *
 * Unclear why standalone uses AudioSourceChannelInfo which contains
 * an AudioBuffer whereas the plugin just gets an AudioBuffer directly
 * in the callback.
 *
 * The two additions are startSample and numSamples in the ChannelInfo which
 * suggest that we can be given an AudioBuffer that is larger than what
 * is actually needed and we're supposed to constrain what and where we consume
 * and fill it.  But I've only ever seen startSample be zero and numSamples
 * match the audio block size.
 *
 * Within the AudioBuffer the documentation and examples around how each
 * channel maps to hardware channels is absolutely fucking terrible.  You're
 * supposed to use AudioDeviceManager to look at the getActiveInputChannels
 * bit vector but the number of channels in the AudioBuffer can actually be
 * smaller than the physical channels, it seems to compress them to remove
 * inactive channels, though that might be related to the number of channels
 * requested of the audio device at startup.  I still don't understand how this
 * shit works, so I'm just going to run some empircal tests and see what happens.
 *
 * Key thing I discovered is that getting the max channels by "getting the highest
 * bit of the active channel vector and adding 1" is flat out wrong.  Always obey
 * the channel count in the AudioBuffer itself.
 * 
 */
void PortAuthority::prepare(const juce::AudioSourceChannelInfo& bufferToFill)
{
    blockSize = bufferToFill.numSamples;
    startSample = bufferToFill.startSample;
    juceBuffer = bufferToFill.buffer;
    resetPorts();
}

/**
 * Get ready to serve buffers when running as a plugin.
 */
void PortAuthority::prepare(juce::AudioBuffer<float>& buffer)
{
    blockSize = buffer.getNumSamples();
    startSample = 0;
    // since the stream->engine->stream callback paradigm is in the way,
    // the reference will go out of scope so we have to save a pointer
    juceBuffer = &buffer;
    resetPorts();
}

void PortAuthority::resetPorts()
{
    for (auto port : ports) {
        port->inputPrepared = false;
        port->outputPrepared = false;
    }

    // void output needs to be cleared whenever it is used
    // void input just needs to be cleared once
    voidPort.outputPrepared = false;
}

/**
 * Get the interleaved input buffer for one port.
 * If the port number is out of range, return an empty buffer so the caller
 * won't crash.
 */
float* PortAuthority::getInput(int port)
{
    float* result = nullptr;
    
    // there are actually two range checks that are done
    // here we check for the the maximum number of ports we support
    // later we check for the maximum port the app/plugin can actually provide
    if (port < 0 || port >= ports.size()) {
        // once this happens, it will happen a lot so only log it once
        if (inputPortRangeErrors == 0) 
          Trace(1, "PortAuthority: Input port out of allowed range %d\n", port);
        inputPortRangeErrors++;

        // this is initialized to zero and stays that way forever
        // in theory things like SamplePlayer could be injecting things into
        // the input buffers but that doesn't happen right now
        if (!voidPort.inputPrepared) {
            clearInterleavedBuffer(voidPort.input);
            voidPort.inputPrepared = true;
        }
        result = voidPort.input;
    }
    else {
        PortBuffer* pb = ports[port];
        if (!pb->inputPrepared) {
            interleaveInput(port, pb->input);
            pb->inputPrepared = true;
        }
        result = pb->input;
    }

    return result;
}

/**
 * Get the interleaved output buffer for one port.
 * If the port number is out of rante, return a scratch buffer so the
 * caller won't crash.
 */
float* PortAuthority::getOutput(int port)
{
    float* result = nullptr;
    
    if (port < 0 || port >= ports.size()) {
        if (outputPortRangeErrors == 0) 
          Trace(1, "PortAuthority: Output port out of allowed range %d\n", port);
        outputPortRangeErrors++;

        // since we ignore whatever goes here, we don't have to clear it first
        result = voidPort.output;
    }
    else {
        PortBuffer* pb = ports[port];
        if (!pb->outputPrepared) {
            // needs to start clean
            clearInterleavedBuffer(pb->output, blockSize);
            pb->outputPrepared = true;
        }
        result = pb->output;
    }

    return result;
}

/**
 * Zero one of our interleaved buffers.
 * Is it still fashionable to use memset?
 *
 * If frames is zero we'll clear the entire buffer.
 * Otherwise we'll do a rather anal optimization and only
 * clear the frames necessary for the current block size.
 */
void PortAuthority::clearInterleavedBuffer(float* buffer, int frames)
{
    if (frames == 0) frames = PortMaxFramesPerBuffer;
    int totalSamples = frames * PortMaxChannels;
      
    // is it still fashionable to use memset?
    memset(buffer, 0, (sizeof(float) * totalSamples));
}

/**
 * Here is where the input shit hits the fan.
 * We've got an unprepared interleaved input buffer and we need to
 * fill it with the left/right samples from Juce AudioBuffer channels
 * that correspond to the given port number.  
 * 
 * I can't even begin to describe what a fucking mess the docs and
 * examples for AudioAppComponent make this look like, but we're going
 * to try to make it simple and assume for our purposes that each Mobius
 * port is one adjacent set of AudioBuffer channels.
 *
 * Don't even get me started on how awful the docs are around this.
 * I expect this will break as I go deeper with hardware channel configurations
 * but it's just impossible to figure out without trying things to
 * see what happens.
 *
 * It seems to be more straightforward for plugins, but the whole "Bus" thing
 * is involved in this and that may need to factor into the decisions here too.
 *
 * A lot of the logic being done here could be done once and saved since we're
 * not expecting channel configuration to change at runtime for each block,
 * but I suppose it could so be safe.
 */
void PortAuthority::interleaveInput(int port, float* result)
{
    int channelOffset = port * 2;
    int maxChannels = juceBuffer->getNumChannels();

    // note that if there is an odd number of available channels, there
    // is probably a mono device configured, could probably prevent that
    // during device configuration but it might be useful to test with a
    // simple headset mic or something

    if (channelOffset >= maxChannels) {
        // desired port number is higher than what can be provided
        if (inputPortHostRangeErrors == 0)
          Trace(1, "PortAuthority: Input port out of range %d\n", port);
        inputPortHostRangeErrors++;
        clearInterleavedBuffer(result, blockSize);
    }
    else {
        auto* leftChannel = juceBuffer->getReadPointer(channelOffset, startSample);
        if (leftChannel == nullptr) {
            // not supposed to happen, but I've seen it when the index was bad
            // might happen with those goofy active channel flags and the AudioBuffer
            // channels were not compressed
            Trace(1, "PortAuthority: Input buffer not available for port %d\n", port);
            clearInterleavedBuffer(result, blockSize);
        }
        else {
            // should have 2 but if there is only one go mono
            auto* rightChannel = leftChannel;
            if (channelOffset + 1 < maxChannels) {
                rightChannel = juceBuffer->getReadPointer(channelOffset + 1, startSample);
                if (rightChannel == nullptr) {
                    // similar unknowns as above
                    Trace(1, "PortAuthority: Input buffer right channel not available for port %d\n", port);
                    rightChannel = leftChannel;
                }
            }
            
            int sampleIndex = 0;
            for (int i = 0 ; i < blockSize ; i++) {
                result[sampleIndex] = leftChannel[i];
                result[sampleIndex+1] = rightChannel[i];
                sampleIndex += 2;
            }
        }
    }
}

/**
 * We're at the end of a long day of audio block processing and are ready
 * to give Juce the fruits of our labor.
 *
 * AudioBuffer is bi-directional, it had input channels and now it wants
 * the output.  Again, the whole active channel folderol for AudioAppComponent
 * comes into play here, as do Busses in the plugin, but I'm starting with
 * some simplifying assumptions and seeing what breaks.
 *
 * Every channel in the AudioBuffer needs to either be filled with Mobius
 * content or cleared if it was not used.
 *
 * Assuming that port bufers can spew into adjacent pairs of Juce channels
 * and that they won't be null.
 */
void PortAuthority::commit()
{
    int maxChannels = juceBuffer->getNumChannels();

    int portNumber = 0;
    int portChannel = 0;
    PortBuffer* port = ports[portNumber];
    
    for (int channel = 0 ; channel < maxChannels ; channel++) {
        auto* destSamples = juceBuffer->getWritePointer(channel, startSample);
        if (destSamples == nullptr) {
            // not sure what this means, may be the active channel shit
            Trace(1, "PortAuthority: Commit found an empty output channel and is giving up\n");
        }
        else {
            float* srcSamples = nullptr;
            if (port != nullptr) {
                if (port->outputPrepared) {
                    srcSamples = port->output;
                    // bump by one if this is the right channel
                    srcSamples += portChannel;
                }
            }

            if (srcSamples == nullptr) {
                // we either don't have a port for this output channel or
                // the engine decided not to put anything into it
                // hmm, I'd use memset here but the "auto" shit makes it unclear
                // how large this is, can you do sizeof(*destSamples) ?
                for (int i = 0 ; i < blockSize ; i++) {
                    destSamples[i] = 0.0f;
                }
            }
            else {
                int srcOffset = 0;
                for (int i = 0 ; i < blockSize ; i++) {
                    destSamples[i] = srcSamples[srcOffset];
                    srcOffset += PortMaxChannels;
                }
            }

            // advance to the next source channel or port
            if (port != nullptr) {
                portChannel++;
                if (portChannel >= PortMaxChannels) {
                    portChannel = 0;
                    portNumber++;
                    if (portNumber < ports.size()) {
                        port = ports[portNumber];
                    }
                    else {
                        // ran off the end, unusual but it could happen
                        // if there was an extremely large audio device and
                        // the configured Mobius port count was lower
                        port = nullptr;
                    }
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// PortBuffer
//
//////////////////////////////////////////////////////////////////////

PortBuffer::PortBuffer()
{
}

PortBuffer::~PortBuffer()
{
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
