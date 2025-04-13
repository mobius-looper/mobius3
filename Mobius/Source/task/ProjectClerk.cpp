
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "../model/SystemConfig.h"
#include "../model/Session.h"

#include "../midi/MidiSequence.h"

#include "../mobius/MobiusInterface.h"
#include "../mobius/TrackContent.h"
#include "../mobius/Audio.h"
#include "../mobius/AudioFile.h"

#include "../Provider.h"
#include "Task.h"

#include "ProjectClerk.h"

ProjectClerk::ProjectClerk(Provider* p)
{
    provider = p;
}

ProjectClerk::~ProjectClerk()
{
}

//////////////////////////////////////////////////////////////////////
//
// Export
//
//////////////////////////////////////////////////////////////////////

/**
 * Commit a TrackContent to the file system.
 * Existing files may be cleared out of the way as that happens.
 * The user will have had the opportunity to cancel if they didn't want overwrites.
 */
int ProjectClerk::writeProject(Task* task, juce::File folder, TrackContent* content)
{
    int fileCount = 0;
    
    // I suppose we could have done the cleanup during the approval phase
    // before we bothered to extract all the data, not expecting this to fail though
    if (folder.existsAsFile()) {
        // the user had the opportunity to preserve this
        if (!folder.deleteFile()) {
            task->addError("Unable to delete file");
            task->addError(folder.getFullPathName());
        }
    }
    else if (folder.isDirectory()) {
        // can leave the directory in place but flush the contents
        cleanFolder(task, folder);
    }
    else {
        juce::Result res = folder.createDirectory();
        if (res.failed()) {
            task->addError("Unable to create project folder");
            task->addError(res.getErrorMessage());
        }
    }

    if (!task->hasErrors()) {

        juce::String manifest;
        manifest += juce::String("project\n");

        for (auto track : content->tracks) {
            
            manifest += juce::String("track ") + juce::String(track->number) + "\n";
                
            for (auto loop : track->loops) {

                manifest += juce::String("loop ") + juce::String(loop->number) + "\n";

                int layerNumber = 0;
                for (auto layer : loop->layers) {
                    
                    juce::String filename = juce::String("track-") + juce::String(track->number) +
                        juce::String("-loop-") + juce::String(loop->number);

                    if (layerNumber > 0)
                      filename += juce::String("-layer-") + juce::String(layerNumber+1);

                    if (layer->midi != nullptr)
                      filename += ".mid";
                    else if (layer->audio != nullptr)
                      filename += ".wav";

                    manifest += filename + "\n";

                    juce::File file = folder.getChildFile(filename);

                    if (layer->midi != nullptr) {
                        // todo: still don't have a .mid file writer
                        task->addWarning("Warning: Unable to save MIDI file");
                        task->addWarning(juce::String("File: ") + file.getFullPathName());
                    }
                    else if (layer->audio != nullptr) {
                        // still using this old gawdawful .wav file tool
                        // need to update this!!
                        Audio* audio = layer->audio.get();
                
                        // when exchanging project files with other applications it can
                        // is important to save the correct sample rate used when they were
                        // recorded.  AudioFile will take the sampleRate stored in the Audio
                        // object
                        audio->setSampleRate(provider->getSampleRate());

                        juce::StringArray writeErrors = AudioFile::write(file, audio);

                        // usually returning no errors means it was created
                        if (writeErrors.size() == 0)
                          fileCount++;
                        else {
                            for (auto err : writeErrors)
                              task->addError(err);
                        }

                        // Could stop on error but proceed and try to get as many of tem
                        // as we can.  If one fails though they probably all will.
                    }

                    layerNumber++;
                }
            }
        }

        juce::File manifestFile = folder.getChildFile("content.mcl");
        if (!manifestFile.replaceWithText(manifest)) {
            task->addError("Unable to write manifest file");
        }
    }

    return fileCount;
}

/**
 * We're about to save project content files in a folder.
 * If the folder is not empty, we have a few options:
 *
 *    1) wipe it completely
 *    2) wipe it of .wav and .mid files but leave the rest
 *    3) just replace the files we need and leave all the rest
 *
 * 2 is a good middle ground and it clears out clutter that may have been
 * left behind if they're using the same destination folder for several saves.
 * It also preserves things like readme.txt or whatever they may choose to put there
 * that aren't files we care about.
 *
 * 3 is the most conservative, but unless we follow the manifest file exactly on import
 * leaving unused files behind might cause them to be loaded on import.
 */
void ProjectClerk::cleanFolder(Task* task, juce::File folder)
{
    cleanFolder(task, folder, "wav");
    cleanFolder(task, folder, "mid");
}

void ProjectClerk::cleanFolder(Task* task, juce::File folder, juce::String extension)
{
    int types = juce::File::TypesOfFileToFind::findFiles;
    bool recursive = false;

    juce::String pattern = juce::String("*.") + extension;
    juce::Array<juce::File> files = folder.findChildFiles(types, recursive, pattern,
                                                          juce::File::FollowSymlinks::no);
    for (auto file : files) {
        if (!file.deleteFile()) {
            task->addError("Unable to delete file");
            task->addError(file.getFullPathName());
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
