/**
 * Utility class for dealing with audio files.
 *
 * Should only be one of these, owned by Supervisor.
 *
 * This has the usual stream-of-conciousness learning comments
 * that should be cleaned up eventually.
 *
 * Go back to how we dealt with sample loading and consolidate
 * everything related to file handling, and Mobius model conversion here.
 *
 */

#include <JuceHeader.h>

#include "util/Trace.h"


#include "Supervisor.h"

// descend into the abyss
#include "mobius/Audio.h"
#include "mobius/MobiusInterface.h"
#include "mobius/WaveFile.h"

#include "MidiClerk.h"
#include "AudioClerk.h"


AudioClerk::AudioClerk(Supervisor* super)
{
    supervisor = super;
    
    // must do this or you get an assertion
    // this gets WAV and AIFF file support, other formats can be registered
    // but not sure how, would be nice to get an MP3 reader
    formatManager.registerBasicFormats();
}

AudioClerk::~AudioClerk()
{
}


/**
 * Read an audio file and convert it to an Audio object suitable
 * for passing down to the engine.
 *
 * This can be used for both Sample loading, and for the Audio
 * inside a Layer inside Loop when loading individual loops from files.
 *
 * This creates a blocked Audio object with blocks from the AudioPool
 * managed by MobiusShell.  This is old and HIGHLY sensitive code.  Do not
 * assume an Audio object is suitable for any purpose other than immediately
 * passing to the engine.  The blocks will eventually be returned to the pool.
 *
 * Since Audio handles it's own breakage of the audio data into blocks, it
 * doesn't really matter how we read it.  You could read the entire file, or
 * read it in chunks to use less contiguous memory.
 *
 * The returned Audio object will remember the AudioPool it came from so if
 * for whatever reason you want to abandon it, just delete it.  Test that...
 *
 * Once this is working, need to rework how SampleConfigs are loaded and sent
 * down so we can prebuild the Audio object rather than having to load it into
 * one ginormous interleaved buffer and then having SamplePlayer break that up
 * and discard it.
 *
 */
Audio* AudioClerk::readFileToAudio(juce::String path)
{
    Audio* audio = nullptr;
    
    const char* cpath = path.toUTF8();
    Trace(2, "AudioClerk: Reading %s\n", cpath);
    juce::File file (path);
    if (!file.existsAsFile()) {
        Trace(1, "AudioClerk: File does not exist %s\n", cpath);
    }
    else {
        // interestingly, this is one of the few classes you have to
        // manually delete, tutorial uses a std::unique_ptr
        // another forum example uses ScopedPointer, which does the same but is deprecated
        // juce::ScopedPointer<juce::AudioFormatReader> reader = formatManager.createReaderFor(file);
        auto reader = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(file));
        if (!reader) {
            Trace(1, "AudioClerk: No registered reader for file %s\n", cpath);
        }
        else {
            traceReader(reader.get());

            // AudioBuffer is the same class used by plugin streams but sadly
            // not AudioAppComponent streams
            juce::AudioBuffer<float> audioBuffer;
            
            // one example shows preallocation, probably not necessary but
            // saves incremental growth?
            // if the file contains more than 2 channels, can we ask for
            // just the first two?
            // WARNING: conversion from juce::int64 to int, possible loss of data
            audioBuffer.setSize(reader->numChannels, (int)reader->lengthInSamples);

            // there are several ways to do this
            // reader->read() and buffer->readFromAudioReader()
            // I imagine they do the same thing
            
            // this from a forum post
            // args are: startSampleInDestBuffer, numSamples, readerStartSample,
            // useReaderLeftChan, useReaderRightChan
            // docs say this will convert the file format into floats and
            // "intelligently cope with mismatches between the number of channels
            // in the reader and the buffer".
            // the two startSample arguments can be used to read ranges
            // of samples, here we read the whole thing
            // unclear what the two flags mean, but seems like what I would always want
            // this appears to be what takes the time and is recommended be performed
            // in a background thread
            // while we still use the old AudioBuffer this would be a GREAT
            // place to read it in chunks and fill Audio buffers rather than read
            // it all in, then convert it
            // WARNING: juce::int64 to int, possible loss of data
            bool status = reader->read(&audioBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
            if (!status)
              Trace(1, "AudioClerk: Reader said no\n");
                
            // at this point the AudioBuffer should have everything
            // break it up
            // todo: rather than reading it all at once, read it in blocks
            // and append each to the Audio
            audio = convertAudioBuffer(audioBuffer);
        }
    }

    return audio;
}

/**
 * Diagnostic utility to dump the information the AudioFormatReader
 * has for a file.  It apparently opens it and reads enough of the
 * file header to get this.
 */
void AudioClerk::traceReader(juce::AudioFormatReader* reader)
{
    Trace(2, "AudioClerk: Pondering reader:\n");
    Trace(2, "  format %s\n", reader->getFormatName().toUTF8());
    // why would this have to be a double?
    Trace(2, "  sampleRate %d\n", (int)(reader->sampleRate));
    Trace(2, "  bitsPerSample %d\n", (int)(reader->bitsPerSample));
    // and this is an int64, we are the world
    Trace(2, "  lengthInSamples %d\n", (int)(reader->lengthInSamples));
    Trace(2, "  numChannels %d\n", (int)(reader->numChannels));
    Trace(2, "  usesFloatingPointData %d\n", (int)(reader->usesFloatingPointData));
    Trace(2, "  metadata:\n");
    juce::StringPairArray& metadata = reader->metadataValues;
    juce::StringArray keys = metadata.getAllKeys();
    for (auto key : keys) {
        juce::String value = metadata[key];
        Trace(2, "    %s = %s\n", key.toUTF8(), value.toUTF8());
    }
}

/**
 * Given a freshly read AudioBuffer, convert it into a Mobius Audio object.
 */
Audio* AudioClerk::convertAudioBuffer(juce::AudioBuffer<float>& audioBuffer)
{
    MobiusInterface* mobius = supervisor->getMobius();
    Audio* audio = mobius->allocateAudio();

    append(audioBuffer, audio);
    
    return audio;
}

/**
 * Append the contents of an AudioBuffer into an Audio
 */
void AudioClerk::append(juce::AudioBuffer<float>& src, Audio* audio)
{
    int bufferFrames = src.getNumSamples();
    int remaining = bufferFrames;
    int consumed = 0;
    
    while (remaining > 0) {

        int transferFrames = remaining;
        if (transferFrames > InterleaveBufferFrames)
          transferFrames = InterleaveBufferFrames;

        interleaveAudioBuffer(src, 0, consumed, transferFrames, interleaveBuffer);

        consumed += transferFrames;
        remaining -= transferFrames;

        AudioBuffer b;
        b.buffer = interleaveBuffer;
        b.frames = transferFrames;
        b.channels = 2;
        audio->append(&b);
    }
}

/**
 * Given an AudioBuffer read samples from each channel array and
 * store them in the interleaved buffer.
 *
 * This could be a static utility shared with JuceMobiusContainer.
 *
 * port is only set when used by plugins, it selects a pair of stereo
 * buffers when the plugin has more than two channels.
 *
 * startSample is the offset within the AudioBuffer to start reading.
 *
 * blockSize is the number of samples to process.
 *
 */
void AudioClerk::interleaveAudioBuffer(juce::AudioBuffer<float>& buffer,
                                       int port, int startSample, int blockSize,
                                       float* resultBuffer)
{
    int channelOffset = port * 2;

    // plugins currently pass this in from 
    // auto inputChannels  = processor->getTotalNumInputChannels();
    // when would this be different from what the AudioBuffer says it has?
    int maxInputs = buffer.getNumChannels();
    
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
            resultBuffer[sampleIndex] = channel1[startSample + i];
            resultBuffer[sampleIndex+1] = channel2[startSample + i];
            sampleIndex += 2;
        }
    }
}

/**
 * Wipe an interleaved buffers of content.
 * Only for the output buffer
 * Note that numSamples here is number of samples in one non-interleaved buffer
 * passed to getNextAudioBlock.  We need to multiply that by the number of channels
 * for the interleaved buffer.
 *
 * Our internal buffer will actually be larger than this (up to 4096 frames) but
 * just do what we need.
 */
void AudioClerk::clearInterleavedBuffer(int numSamples, float* buffer)
{
    // is it still fashionable to use memset?
    int totalSamples = numSamples * 2;
    memset(buffer, 0, (sizeof(float) * totalSamples));
}

//////////////////////////////////////////////////////////////////////
//
// Drag and Drop
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by MainWindow, TrackStrip, or LoopStack  when it receives a shipment
 * of dropped files.
 *
 * track and loop will both be zero when dropping outside the track strips.
 * track is set when over a track strip but not over the loop stack.
 * both track and loop are set when over the loop stack.
 *
 * todo: decide how to deal with multiples.  Would be nice to support
 * that for main and strip drop, just fill the loops we can.  When dropping
 * over a single loop, can only take the first one.
 *
 * Until we have a better distributor, this will handle both audio drops and midi drops.
 * Could just as well handle scripts here too then MainWindow wouldn't need to deal with it.
 */
void AudioClerk::filesDropped(const juce::StringArray& files, int track, int loop)
{
    juce::StringArray audioFiles;
    juce::StringArray midiFiles;
    
    for (auto path : files) {
        juce::File f (path);
        juce::String ext = f.getFileExtension();

        if (ext == juce::String(".wav"))
          audioFiles.add(path);
        else if (ext == juce::String(".mid") || ext == juce::String(".smf"))
          midiFiles.add(path);
    }

    if (audioFiles.size() > 0) {
        // todo: multiples
        juce::String path = files[0];

        Audio* audio = readFileToAudio(path);
        if (audio != nullptr) {

            // writeAudio(audio, "dropfile.wav");
            
            MobiusInterface* mobius = supervisor->getMobius();
            // zero means "active" for both loop and track
            mobius->installLoop(audio, track, loop);
        }
    }
    else if (midiFiles.size() > 0) {
        // redirect
        MidiClerk* mclerk = supervisor->getMidiClerk();
        mclerk->filesDropped(midiFiles, track, loop);
    }
}

/**
 * For testing purposes, write the Audio to a file using
 * the old WaveFile tool.
 */
void AudioClerk::writeAudio(Audio* audio, const char* filename)
{
    juce::File root = supervisor->getRoot();
    juce::File file = root.getChildFile(filename);
    const char* cpath = file.getFullPathName().toUTF8();

    // here on down is taken from the old Audio::write method
	int error = 0;

	WaveFile* wav = new WaveFile();
	wav->setChannels(2);
	wav->setFrames(audio->getFrames());
	wav->setFormat(WAV_FORMAT_IEEE);
	wav->setFile(cpath);

	error = wav->writeStart();
	if (error) {
		Trace(1, "AudioClerk: Error writing file %s: %s\n", filename,
			  wav->getErrorMessage(error));
	}
	else {
		// write one frame at a time not terribly effecient but messing
		// with blocking at this level isn't going to save much
		AudioBuffer b;
		float buffer[4];
		b.buffer = buffer;
		b.frames = 1;
		b.channels = 2;

		for (long i = 0 ; i < audio->getFrames() ; i++) {
			memset(buffer, 0, sizeof(buffer));
			audio->get(&b, i);
			wav->write(buffer, 1);
		}

		error = wav->writeFinish();
		if (error) {
			Trace(1, "AudioClerk: Error finishing file %s: %s\n", filename,
				  wav->getErrorMessage(error));
		}
	}

	delete wav;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
