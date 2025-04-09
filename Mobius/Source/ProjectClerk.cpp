
#include <JuceHeader.h>

#include "Provider.h"

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
 * Export ultimately needs the full path to a project folder.
 * It starts by assuming this lives under the configured userFileFolder system parameter
 * and if that is not set defaults to the installation directory under a folder named "projects".
 *
 * Within that containing folder, the project folder will have the name of the crrently loaded
 * session.  If that name already exist is it qualified with numbers until it is unique.
 *
 * The result is then presented to the user for approval.   They may either accept it, or
 * ask to choose a different folder.
 *
 * If the user's choice already exists and has things in it, the user must confirm the
 * replacement of the existing content.
 */
void ProjectClerk::exportProject()
{
    juce::StringArray errors;
    juce::File projectContainer;


    juce::File projectFolder = locateProjectDestination();
    if (projectFolder != juce::File()) {
    }
}

/**
 * The most annoying part about all this process is determining
 * where the the project goes.  So many options...
 */
juce::File locateProjectDestination()
{
    juce::File projectFolder;

    // first see if we can auto-create it within the configured user directory
    SystemConfig* scon = provider->getSystemConfig();
    juce::String userFolder = scon->getString("userFileFolder");
    
    if (userFolder.length() > 0) {
        juce::File f (userFolder);
        if (!f.isDirectory()) {
            errors.add(juce::String("Invalid value for User File Folder parameter"));
            errors.add(userFolder);
            userFolder = "";
        }
        else {
            projectContainer = f;
        }
    }
    
    if (userFolder.length() == 0) {
        juce::File root = provider->getRoot();
        juce::File defaultContainer = root.getChildFile("projects");
        if (defaultContainer.isDirectory()) {
            projectContainer = defaultContainer;
        }
        else {
            juce::Result res = defaultContainer.createDirectory();
            if (res.failed()) {
                errors.add("Unable to create default projects folder");
                errors.add(res.getErrorMessage());
            }
            else {
                projectContainer = defaultContainer;
            }
        }
    }

    juce::File projectFolder;
    if (errors.size() == 0) {
        Session* s = provider->getSession();
        juce::String projectName = s->getName();
        juce::File possibleFolder = projectContainer.getChildFile(projectName);
        if (possibleFolder.existsAsFile() ||
            possibleFolder.isDirectory()) {

            juce::File qualifiedFolder = qualifyName(possibleFolder, errors);
            if (qualifiedFolder != juce::File())
              projectFolder = qualifiedFolder;
        }
        else {
            projectFolder = possibleFolder;
        }
    }
    
    if (errors.size() == 0) {
        
        // verify chosen folder, and allow user to pick a different one
    }


    if (errors.size() == 0) {
        exportProject(projectFolder, errors);
    }
}

void ProjectClerk::exportProject(juce::file projectFolder, juce::StringArray errors)
{
    MobiusInterface* mobius = provider->getMobius();

    // todo: option for this
    bool includeLayers = false;
    
    TrackContent* content = mobius->getTrackContent(includeLayers);
    if (content == nullptr) {
        errors.add("Mobius engine did not return track content");
    }
    else {
        for (auto track : content->tracks) {
            exportTrack(projectFolder, track, errors);
            if (errors.s9ze() > 0)
              break;
        }

        if (errors.size() == 0) {
            writeManfest(projectFolder, content);
        }
    }
}

void ProjectClerk::exportTrack(juce::File projectFolder, TrackContent::Track* track,
                               juce::StringArray errors)
{
    juce::String baseName = juce;:String("track-") + juce::String(track->number) + "-";

    int number = 1;
    for (auto loop : track->loops) {
        exportLoop(projectFolder, baseName, number, loop, errors);
        if (errors.size() > 0)
          break;
        number++;
    }
}

void ProjectClerk::exportLoop(juce::File projectFolder, juce::String baseName,
                              TrackContent::Loop* loop, int number,
                              juce::StringArray errors)
{
    // the first layer which is usually the only one does not have a -layer suffix
    juce::String loopFile = baseName + "loop-" + juce::String(number);

    if (loop->layers.size() > 0) {
        TrackContent::Layer* first = loop->layers[0];
        exportLayer(projectFolder, loopFile, first, 0, errors);

        for (int i = 1 ; i < loop->layers.size() ; i++) {
            TrackContent::Layer* layer = loop->layers[i];
            exportLayer(projectFolder, loopFile, layer, i, errors);
            if (errors.size() > 0)
              break;
        }
    }
}

void ProjectClerk::exportLoop(juce::File projectFolder, juce::String baseName,
                              TrackContent::Layer* layer, int number,
                              juce::StringArray errors)
{
    juce::String layerFile = baseName;
    if (number > 0)
      layerFile += juce::String("-layer-") + juce::String(number+1);

    if (layer->audio != nullptr) {
        juce::String fullFile = layerFile + ".wav";
        writeAudio(projectFolder, fullFile, layer->audio.get(), errors);
    }
    else if (layer->midi != nullptr) {
        juce::String fullFile = layerFile + ".mid";
        writeMidi(projectFolder, fullFile, layer->midi.get(), errors);
    }
    else {
        Trace(1, "ProjectClerk: TrackContent contained an empty layer");
    }
}

void ProjectClerk::writeAudio(juce::File projectFolder, juce::String filename,
                              Audio* audio, juce::StringArray errors)
{
    
}

void ProjectClerk::writeMidi(juce::File projectFolder, juce::String filename,
                             MidiSequence* seq, juce::StringArray errors)
{
}

void ProjectClerk::writeManifest(juce::File projectFolder, TrackContent* c)
{
}

//////////////////////////////////////////////////////////////////////
//
// Import
//
//////////////////////////////////////////////////////////////////////

void ProjectClerk::importProject()
{
}

