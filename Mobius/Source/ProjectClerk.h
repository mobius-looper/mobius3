/**
 * Utility to manage exporting and importing projects.
 *
 * Eventual replacement for the older ProjectFiler
 *
 */

#pragma once

#include <JuceHeader.h>

#include "mobius/TrackContent.h"

#include "ui/common/YanDialog.h"

class ProjectWorkflow
{
  public:

    juce::File projectContainer;
    juce::File projectFolder;

    std::unique_ptr<class TrackContent> content;
    
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
    void locateProjectDestination();
    juce::File generateUnique(juce::String desired);
    void chooseDestination();
    void showResult();
    void cancelWorkflow();

    // final file creation
    void compileProject();
    void writeProject();
    void cleanFolder(juce::File folder);
    void cleanFolder(juce::File folder, juce::String extension);

};
