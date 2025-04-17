
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "../model/SystemConfig.h"
#include "../model/Session.h"

#include "../midi/MidiSequence.h"

#include "../mobius/MobiusInterface.h"
#include "../mobius/TrackContent.h"
#include "../mobius/Audio.h"
#include "../mobius/AudioFile.h"
#include "../mobius/AudioPool.h"

#include "../Provider.h"
#include "Task.h"

#include "SnapshotClerk.h"

SnapshotClerk::SnapshotClerk(Provider* p)
{
    provider = p;
}

SnapshotClerk::~SnapshotClerk()
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
int SnapshotClerk::writeSnapshot(Task* task, juce::File folder, TrackContent* content)
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
            task->addError("Unable to create snapshot folder");
            task->addError(res.getErrorMessage());
        }
    }

    if (!task->hasErrors()) {

        juce::String manifest;
        manifest += juce::String("snapshot\n");

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
                
                        // when exchanging snapshot files with other applications it can
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
 * We're about to save snapshot content files in a folder.
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
void SnapshotClerk::cleanFolder(Task* task, juce::File folder)
{
    cleanFolder(task, folder, "wav");
    cleanFolder(task, folder, "mid");
}

void SnapshotClerk::cleanFolder(Task* task, juce::File folder, juce::String extension)
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

//////////////////////////////////////////////////////////////////////
//
// Import Snapshot
//
//////////////////////////////////////////////////////////////////////

TrackContent* SnapshotClerk::readSnapshot(Task* task, juce::File file)
{
    (void)task;
    (void)file;
    return nullptr;
}

//////////////////////////////////////////////////////////////////////
//
// Import Old Project
//
//////////////////////////////////////////////////////////////////////

TrackContent* SnapshotClerk::readProject(Task* task, juce::File file)
{
    TrackContent* content = nullptr;
    
    // not using the hideous old parser, should be well formed enough
    juce::String xml = file.loadFileAsString();
    if (xml.length() == 0) {
        task->addError("Empty file");
    }
    else {
        juce::XmlDocument doc(xml);
        std::unique_ptr<juce::XmlElement> docel = doc.getDocumentElement();
        if (docel == nullptr) {
            task->addError("XML Parsing Error");
            task->addError(doc.getLastParseError());
        }
        else if (!docel->hasTagName("Project")) {
            task->addError("Not an old Project XML file");
        }
        else {
            content = new TrackContent();

            juce::File project = file.getParentDirectory();

            for (auto* el : docel->getChildIterator()) {
                if (el->hasTagName("Track")) {
                    parseOldTrack(task, project, content, el);
                }
                else {
                    // don't die I guess
                    Trace(1, "SnapshotClerk: Unexpected element in old Project file: %s",
                          el->getTagName().toUTF8());
                }
            }
        }
    }

    return content;
}

/**
 * A Track element used to contain snapshots of a few parameters like the levels
 * and a flag indiciating whether it was active.
 */
void SnapshotClerk::parseOldTrack(Task* task, juce::File project, TrackContent* content, juce::XmlElement* root)
{
    TrackContent::Track* track = new TrackContent::Track();
    track->active = root->getBoolAttribute("active");

    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName("Loop")) {
            parseOldLoop(task, project, track, el);
        }
        else {
            Trace(1, "SnapshotClerk: Unexpected element in old Project file: %s",
                  el->getTagName().toUTF8());
        }
    }

    if (track->loops.size() > 0)
      content->tracks.add(track);
    else
      delete track;
}

void SnapshotClerk::parseOldLoop(Task* task, juce::File project, TrackContent::Track* track, juce::XmlElement* root)
{
    TrackContent::Loop* loop = new TrackContent::Loop();
    loop->active = root->getBoolAttribute("active");
    
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName("Layer")) {
            parseOldLayer(task, project, loop, el);
        }
        else {
            Trace(1, "SnapshotClerk: Unexpected element in old Project file: %s",
                  el->getTagName().toUTF8());
        }
    }

    // the only reason to return an empty loop is if you wanted
    // to convey the active flag
    if (loop->layers.size() > 0)
      track->loops.add(loop);
    else
      delete loop;
}

void SnapshotClerk::parseOldLayer(Task* task, juce::File project, TrackContent::Loop* loop, juce::XmlElement* root)
{
    // the important things in a Layer are the file path and the cycle count
    juce::String path = root->getStringAttribute("audio");
    int cycles = root->getIntAttribute("cycles");

    // .mob files have historically used absolute paths but it is
    // quite common for those to be wrong when changing machines
    // or exchanging projects with someone else
    // I'd like to enforce that the .wav files be relative
    // to the .mob file, though this may break old projects where the user
    // manually moved the files somewhere else and edited the .mob file
    // unlikely though

    // this doesn't seem to do enough, was hoping it would mutate slashes but
    // it doesn't
    juce::String convpath = juce::File::createLegalPathName(path);

    // hack, if the path already appears to be relative, don't stick it
    // in a juce::File since juce will break with an annoying assertion
    juce::File file;
    if (looksAbsolute(convpath)) {
        juce::File jpath = juce::File(convpath);
        // todo: here would be an option to preserve the original path, or redirect
        // ito the project root
        file = project.getChildFile(jpath.getFileName());
    }
    else {
        // shouldn't be here but some people may have hand edited the manifest
        file = project.getChildFile(convpath);
    }

    // and now we've got our old audio file reader
    // continue using that till I can test a replacement

    // continue to use the ancient Audio/AudioPool system
    // this really needs a rewrite
    MobiusInterface* mobius = provider->getMobius();
    AudioPool* pool = mobius->getAudioPool();
    juce::StringArray errors;
    Audio* audio = AudioFile::read(file, pool, errors);

    if (errors.size() > 0) {
        for (auto e : errors)
          task->addError(e);
    }
    else if (audio == nullptr) {
        task->addError("Unable to read .wav file");
    }
    else {
        TrackContent::Layer* layer = new TrackContent::Layer();
        layer->audio.reset(audio);
        layer->cycles = cycles;
        loop->layers.add(layer);
    }
}

/**
 * Return true if this smells like an absolute path so we can avoid
 * an annoying Juce assertion if you try to construct a juce::File with
 * a relative path.
 *
 * On mac, this would start with '/'
 * On windows, this would have ':' in it somewhere, typically
 * as the second character but I guess it doesn't matter.
 *
 * I suppose we could be more accurate by looking at the actual runtime
 * architecture.  It actually would be nice in my testing to auto-convert
 * from one style to another so files in source control like mobius.xml ScriptConfig
 * can easly slide between them.
 * 
 */
bool SnapshotClerk::looksAbsolute(juce::String path)
{
    return path.startsWithChar('/') || path.containsChar(':');
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
