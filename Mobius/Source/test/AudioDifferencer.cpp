/**
 * TestDriver tools for comparing captured audio files.
 *
 * The core comparison code is very old and hacks around
 * noise in float math by doing an integer conversion out to
 * a certain level of bit precision.  There has to be a better
 * way to accomplish this but once anything is done to a float
 * beyond just copying it from one place to another, binary
 * comparisons seem to be unreliable and machine specific.
 *
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../util/Util.h"

#include "../mobius/Audio.h"
#include "../mobius/AudioPool.h"
#include "../mobius/AudioFile.h"

#include "TestDriver.h"
#include "AudioDifferencer.h"

// forget where this came from
#define MaxAudioChannels 4

AudioDifferencer::AudioDifferencer(TestDriver* td)
{
    driver = td;
}

AudioDifferencer::~AudioDifferencer()
{
}

/**
 * Current interface that operates from a KernelEvent from
 * a test script.
 */
void AudioDifferencer::diff(juce::File result, juce::File expected, bool reverse)
{
    // hmm, getFullPathName().toUTF() seems to become unstable
    // after you call anything else on the FIle, like existsAsFile
    // so can't capture those early, have to wait until needed
    // and not expect them to live long

    if (!result.existsAsFile()) {
        const char* path = result.getFullPathName().toUTF8();
        Trace(1, "Diff result file not found: %s\n", path);
    }
    else if (!expected.existsAsFile()) {
        // expected file not there, could bootstrap it?
        const char* path = expected.getFullPathName().toUTF8();
        Trace(1, "Diff expected file not found: %s\n", path);
    }
    else if (result.getSize() != expected.getSize()) {
        const char* path1 = result.getFullPathName().toUTF8();
        const char* path2 = expected.getFullPathName().toUTF8();
        Trace(1, "Diff files differ in size: %s, %s\n", path1, path2);
    }
    else {
        // reading files requires a pool
        AudioPool* pool = driver->getAudioPool();
        Audio* a1 = AudioFile::read(result, pool);
        Audio* a2 = AudioFile::read(expected, pool);
        
        const char* path1 = result.getFullPathName().toUTF8();
        const char* path2 = expected.getFullPathName().toUTF8();

        if (a1->getFrames() != a2->getFrames()) {
            Trace(1, "Diff file frame counts differ %s, %s\n", path1, path2);
            Trace(1, "  Frames %ld %ld\n", (long)a1->getFrames(), (long)a2->getFrames());
        }
        else if (a1->getChannels() != 2) {
            Trace(1, "Diff file channel count not 2: %s\n", path1);
        }
        else if (a2->getChannels() != 2) {
            Trace(1, "Diff file channel count not 2: %s\n", path2);
        }

        diffAudio(path1, a1, path2, a2, reverse);

        delete a1;
        delete a2;
    }
}

/**
 * The original implementation
 */
void AudioDifferencer::diffAudio(const char* path1, Audio* a1,
                                 const char* path2, Audio* a2,
                                 bool reverse)
{
    // formerly checked channel counts, which were always 2 and in
    // newer code may be unset, so just ass
    int channels = 2;
    AudioBuffer b1;
    float f1[MaxAudioChannels];
    b1.buffer = f1;
    b1.frames = 1;
    b1.channels = channels;

    AudioBuffer b2;
    float f2[MaxAudioChannels];
    b2.buffer = f2;
    b2.frames = 1;
    b2.channels = channels;

    long psn2 = (reverse) ? a2->getFrames() - 1 : 0;

    bool different = false;
    bool checkFloats = false;
    for (int i = 0 ; i < a1->getFrames() && !different ; i++) {

        memset(f1, 0, sizeof(f1));
        memset(f2, 0, sizeof(f2));
        a1->get(&b1, i);
        a2->get(&b2, psn2);
			
        for (int j = 0 ; j < channels && !different ; j++) {
            
            // sigh, due to rounding errors it is 
            // impossible to reliably assume that
            // x + y - y = x with floats, so coerce
            // to an integer to do the comparion
            if (checkFloats) {
                if (f1[j] != f2[j]) {
                    // sigh, don't have Trace signatures that use floats
                    char msg[1024];
                    snprintf(msg, sizeof(msg), "Raw float difference at frame %d: %f %f: %s, %s\n",
                            i, f1[j], f2[j], path1, path2);
                    Trace(1, msg);
                }
            }

            // coerce to an int
            // 24 bit is too much, but 16 is too small
            // 16 bit signed (2^15)
            //float precision = 32767.0f;
            // 24 bit signed (2^23)
            //float precision = 8388608.0f;
            // 20 bit
            float precision = 524288.0f;
								
            int i1 = (int)(f1[j] * precision);
            int i2 = (int)(f2[j] * precision);

            // some of the jump tests involving rate shift have
            // an off by 1 difference, in an otherwise good wave
            // tolerate 1, but probably need to tune this, or at least allow
            // some threshold of scattered minor errors
            // also need to explore denormals like I've seen in some Juce examples
            // but this won't fix old test files
            int tolerance = 2;
            if (i1 != i2) {
                int delta = abs(i1 - i2);
                if (delta <= tolerance)
                  i1 = i2;
            }

            if (i1 != i2) {
                char msg[1024];
                snprintf(msg, sizeof(msg), "Files differ at frame %d: %d %d: %s, %s\n",
                        i, i1, i2, path1, path2);
                Trace(1, msg);
                different = true;
            }
        }

        if (reverse)
          psn2--;
        else
          psn2++;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Analyze
//
//////////////////////////////////////////////////////////////////////

/**
 * Special interface for testing differencing code from an action button.
 */
void AudioDifferencer::analyze(juce::File result, juce::File expected)
{
    if (!result.existsAsFile()) {
        const char* path = result.getFullPathName().toUTF8();
        Trace(1, "Results file not found: %s\n", path);
    }
    else if (!expected.existsAsFile()) {
        // expected file not there, could bootstrap it?
        const char* path = expected.getFullPathName().toUTF8();
        Trace(1, "Expected file not found: %s\n", path);
    }
    else if (result.getSize() != expected.getSize()) {
        const char* path1 = result.getFullPathName().toUTF8();
        const char* path2 = expected.getFullPathName().toUTF8();
        Trace(1, "Files differ in size: %s, %s\n", path1, path2);
    }
    else {
        // reading files requires a pool
        AudioPool* pool = driver->getAudioPool();
        Audio* a1 = AudioFile::read(result, pool);
        Audio* a2 = AudioFile::read(expected, pool);
        
        const char* path1 = result.getFullPathName().toUTF8();
        const char* path2 = expected.getFullPathName().toUTF8();

        if (a1->getFrames() != a2->getFrames()) {
            Trace(1, "Diff file frame counts differ %s, %s\n", path1, path2);
            Trace(1, "  Frames %ld %ld\n", (long)a1->getFrames(), (long)a2->getFrames());
        }
        else if (a1->getChannels() != 2) {
            Trace(1, "Diff file channel count not 2: %s\n", path1);
        }
        else if (a2->getChannels() != 2) {
            Trace(1, "Diff file channel count not 2: %s\n", path2);
        }
        else {

            analyze(a1, a2);
        }
    }
}

void AudioDifferencer::analyze(Audio* a1, Audio* a2)
{
    // formerly checked channel counts, which were always 2 and in
    // newer code may be unset, so just ass
    int channels = 2;
    AudioBuffer b1;
    float f1[MaxAudioChannels];
    b1.buffer = f1;
    b1.frames = 1;
    b1.channels = channels;

    AudioBuffer b2;
    float f2[MaxAudioChannels];
    b2.buffer = f2;
    b2.frames = 1;
    b2.channels = channels;

    // not doing reverse compare yet
    bool reverse = false;
    int psn2 = (reverse) ? (int)(a2->getFrames() - 1) : 0;

    // attempt at fuzzy matching that never went anywhere
    // comment out to avoid compiler warnings
/*
    int maxRange = 2;
    int rangeAlarm = 10;
    int maxDiffs = 10;
    int totalDiffs = 0;
    int totalPartialDiffs = 0;
*/
    
    int maxLines = 1000;
    int lines = 0;

    bool stop = false;
    juce::String buffer;

    for (int i = 0 ; i < a1->getFrames() && !stop ; i++) {

        memset(f1, 0, sizeof(f1));
        memset(f2, 0, sizeof(f2));
        a1->get(&b1, i);
        //a2->get(&b2, psn2);
        a2->get(&b2, i);

        // channel 0
        float sample1 = f1[0];
        float sample2 = f2[0];

        if (sample1 != sample2) {


            // 24 bit is too much, but 16 is too small
            // 16 bit signed (2^15)
            //float precision = 32767.0f;
            // 24 bit signed (2^23)
            //float precision = 8388608.0f;
            // 20 bit
            float precision = 524288.0f;
								
            int i1 = (int)(sample1 * precision);
            int i2 = (int)(sample2 * precision);
            int delta = i1 - i2;

            if (delta > 0) {
            
                buffer += juce::String(i) + ": " + juce::String(sample1) + " "  + juce::String(sample2) + " " + 
                    juce::String(i1) + " " + juce::String(i2) + " " + juce::String(abs(delta)) + "\n";
        
                lines++;
                stop = (lines >= maxLines);
            }
        }
        
        if (reverse)
          psn2--;
        else
          psn2++;
    }

    juce::File out ("c:/dev/jucetest/UI/Source/diffout.txt");
    
    out.replaceWithText(buffer);
    
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
