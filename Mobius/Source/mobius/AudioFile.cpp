/**
 * Simple utility to read/write Audio objects from/to files.
 *
 * Relies on having an AudioPool for reading and the returned
 * Audio object should be returned to that pool.  Audio is unfortunately
 * not quite a standalone object yet.
 *
 * In old code file handling was strewn about in lots of places, often embedded
 * deep within things like Audio::write called from wherever.  Slowly trying
 * to push file handling up to the shell, and start using these exclusively.
 * Once the dust settles look at what Juce offers for more flexible file formatting.
 *
 */

#include "../util/Trace.h"

#include "Audio.h"
#include "AudioPool.h"
#include "WaveFile.h"

#include "AudioFile.h"

/**
 * Interface I started with that didn't do an error list
 */
void AudioFile::write(juce::File file, Audio* a)
{
    juce::StringArray errors;
    write(file, a, errors);
}

/**
 * Write an audio file using the old tool.
 * This is an adaptation of what used to be in Audio::write()
 * which no longer exists, but still sucks because it does this
 * a sample at a time rather than blocking.  Okay for initial testing
 * but you can do better.
 */
void AudioFile::write(juce::File file, Audio* a, juce::StringArray errors)
{
    // Old code gave the illusion that it supported something other than 2
    // channels but this was never tested.  Ensuing that this all stays
    // in sync and something forgot to set the channels is tedius, just
    // force it to 2 no matter what Audio says
    //int channels = a->getChannels();
    int channels = 2;
    int frames = (int)(a->getFrames());
    
	WaveFile* wav = new WaveFile();
	wav->setChannels(channels);
	wav->setFrames(frames);
    // comes from WaveFile.h
    // other format is PCM, but I don't think the old writer supported that?
	wav->setFormat(WAV_FORMAT_IEEE);
    // this was how we conveyed the file path
    const char* path = file.getFullPathName().toUTF8();
	wav->setFile(path);

    // the old tool will not auto-create parent directories, let's
    // use Juce to handle that now
    file.create();
    
	int error = wav->writeStart();
	if (error) {
		Trace(1, "Error writing file %s: %s\n", path, 
			  wav->getErrorMessage(error));

        errors.add(juce::String("Error writing file ") +
                   juce::String(path) + ": " +
                   juce::String(wav->getErrorMessage(error)));
	}
	else {
		// write one frame at a time not terribly effecient but messing
		// with blocking at this level isn't going to save much
		AudioBuffer b;
        // not sure where this constant went but we don't support more than 2 anyway
		//float buffer[AUDIO_MAX_CHANNELS];
		float buffer[MaxAudioChannels];
		b.buffer = buffer;
		b.frames = 1;
		b.channels = channels;

		for (long i = 0 ; i < frames ; i++) {
			memset(buffer, 0, sizeof(buffer));
			a->get(&b, i);
			wav->write(buffer, 1);
		}

		error = wav->writeFinish();
		if (error) {
			Trace(1, "Error finishing file %s: %s\n", path, 
				  wav->getErrorMessage(error));

            errors.add(juce::String("Error finishing file ") +
                       juce::String(path) + ": " +
                       juce::String(wav->getErrorMessage(error)));
		}
    }

    delete wav;
}

/**
 * Read a .wav file from the file system.
 *
 * Uses two old components, WaveFile reads the float* data
 * and leaves it in one big buffer.
 *
 * Audio::append takes that and copies it into a collection
 * of segmented AudioBuffers.  All of this needs to come
 * out of an AudioPool because the old interface thinks
 * delete on an Audio just returns the buffers to a pool
 * rather than deleting them.
 *
 * This isn't always necessary in some cases, like unit tests
 * and Sample loading.  But Audio/AudioPool is old and
 * sensitive and I don't want to mess with how it expects
 * memory right now.
 */
Audio* AudioFile::read(juce::File file, AudioPool* pool)
{
    Audio* audio = nullptr;

    const char* filepath = file.getFullPathName().toUTF8();
	WaveFile* wav = new WaveFile();
	int error = wav->read(filepath);
	if (error) {
		Trace(1, "Error reading file %s %s\n", filepath, 
			  wav->getErrorMessage(error));
	}
    else {
        // this is the interesting part
        // create an Audio and fill it with the float data
        // this does not take ownership of the source data,
        // it copies it into a set of segmented AudioBuffers
        // everything has to come out of the AudioPool currently
        audio = pool->newAudio();

        AudioBuffer b;
        b.buffer = wav->getData();
        b.frames = wav->getFrames();
        b.channels = 2;
        // I think we used to capture the sample rate here too
        
        audio->append(&b);
    }

    delete wav;

    return audio;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
