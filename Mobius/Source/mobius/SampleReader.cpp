/**
 * Utiltiies for loading Sample files.
 * This could be inside MobiusInterface, but I'd like to
 * keep file handling above to limit Juce and OS awareness.
 * This could also be a service provided by the MobiusContainer
 * which would fit with other things.
 *
 * update: I've moved toward doing file handling in MobiusShell
 * because it's hard to avoid and lightens the load on the UI
 * should stop using this and just send down the new
 * LoadSamples intrinsic function in a UIAction
 * 
 */
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/SampleConfig.h"

#include "WaveFile.h"
#include "SampleReader.h"

SampleConfig* SampleReader::loadSamples(SampleConfig* src)
{
    SampleConfig* loaded = new SampleConfig();
    if (src != nullptr) {
        for (auto srcSample : src->getSamples()) {
            if (srcSample->file.length() > 0) {

                juce::File file(srcSample->file);
                if (!file.exists()) {
                    Trace(1, "Sample file not found: %s", srcSample->file.toUTF8());
                }
                else {
                    Sample* copySample = new Sample(srcSample);
                    bool success = readWaveFile(copySample, file);
                    if (success)
                      loaded->add(copySample);
                }
            }
        }
    }

    return loaded;
}

/**
 * Should try to learn juce::AudioFormatReader but
 * punting for now and using my old utilitity.  I know
 * it works and i'm not sure if it was doing stereo
 * sample interleaving in the same way.
 */
bool SampleReader::readWaveFile(Sample* dest, juce::File file)
{
    bool success = false;

    const char* filepath = file.getFullPathName().toUTF8();
	WaveFile* wav = new WaveFile();
	int error = wav->read(filepath);
	if (error) {
		Trace(1, "Error reading file %s %s\n", filepath, 
			  wav->getErrorMessage(error));
	}
	else {
        dest->setData(wav->stealData(), (int)(wav->getFrames()));
        success = true;
	}
	delete wav;
    return success;
}
