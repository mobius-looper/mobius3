/**
 * Utility to manage exporting and importing projects.
 *
 * Eventual replacement for the older ProjectFiler
 *
 */

#pragma once

#include <JuceHeader.h>

#include "mobius/TrackContent.h"
//#include "mobius/Audio.h"
//#include "midi/MidiSequence.h"

#include "ui/common/YanDialog.h"

/**
 * Transient structure to represent the files files discovered during import
 * or the files to create during export.
 */
class ProjectPlan
{
  public:

    // file to be read or created
    class File {
      public:
        File() {}
        // todo: override this and use the pools
        ~File() {}
        juce::File file;
        std::unique_ptr<class Audio> audio;
        std::unique_ptr<class MidiSequence> midi;
    };

    // the root of the project folder
    juce::File folder;

    // errors accumulated during discovery
    juce::StringArray errors;

    juce::OwnedArray<File> files;
};

class ProjectWorkflow
{
  public:

    std::unique_ptr<ProjectPlan> plan;


    juce::File projectContainer;
    juce::File projectFolder;
    
    juce::StringArray errors;
    juce::StringArray warnings;
    
    bool warnOverwrite = false;

    bool hasErrors() {
        return (errors.size() > 0);
    }
    
};

class ProjectClerk : public YanDialog::Listener
{
  public:

    ProjectClerk(class Provider* p);
    ~ProjectClerk();

    /**
     * Primary entry point for exporting the current session contents as a project.
     * This is currently doing a live capture of the session in memory, it does not simply
     * export content files stored in the session.  May want both.
     */
    void exportProject();
    
    void importProject();

    // internal use but need to be public for the async notification?
    // void continueWorkflow(bool cancel);
    
    void yanDialogClosed(YanDialog* d, int button);

  private:

    class Provider* provider = nullptr;
    YanDialog confirmDialog {this};
    YanDialog resultDialog {this};
    
    std::unique_ptr<ProjectWorkflow> workflow;

    // workflow steps
    void confirmDestination();

    // final file creation
    void compileProject();
    void addTrack(TrackContent::Track* track);
    void addLoop(juce::String baseName, TrackContent::Loop* loop, int number);
    void addLayer(juce::String baseName, TrackContent::Layer* layer, int number);

    void locateProjectDestination();
    juce::File generateUnique(juce::String desired);

    void writePlan();
    void cleanFolder(juce::File folder);
    void cleanFolder(juce::File folder, juce::String extension);
    
    void chooseDestination();
    void showResult();
    void cancelWorkflow();

};
