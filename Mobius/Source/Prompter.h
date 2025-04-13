/**
 * Newer system component that deals with async file choosers,
 * and confirmation dialogs.
 *
 * These are currently strewn about in several places, and I'd like to have
 * some order to them.  Since this is owned by Supervisor there is no danger of
 * the UI component that launched them being deleted by the time the async
 * window is closed.
 *
 * See how this evolves...
 *
 * Makes use of the new Pathfinder, which could probably be merged with this unless
 * Pathfinder has something else to do besides file chooser folders.
 *
 * !! UPDATE
 *
 * This needs to be redesigned in light of the new Task subsystem.
 * Anything that needs to choose and act on files should be a Task now.
 */

#pragma once

#include <JuceHeader.h>

#include "Services.h"

class Prompter : public FileChooserService
{
  public:

    class Handler {
      public:
        virtual ~Handler() {}
        virtual void prompterHandleFile(juce::String path) = 0;
    };
    
    Prompter(class Provider* p);
    ~Prompter();

    void importScripts();
    void deleteScript(juce::String path);

    // FileChooserService
    void fileChooserRequestFolder(juce::String purpose, FileChooserService::Handler* handler) override;
    void fileChooserCancel(juce::String purpose) override;
    
    void runMcl();

    void logActiveHandlers();
    
  private:

    class Provider* provider = nullptr;

    // let's just keep one of these around and prevent concurrent access
    // though we could have an array of them for each purpose
    std::unique_ptr<juce::FileChooser> chooser;

    juce::HashMap<juce::String,FileChooserService::Handler*> fileChooserRequests;

    void startScriptImport();
    void finishScriptImport(juce::Array<juce::File> files);
    void finishRunMcl(juce::Array<juce::File> files);

    void chooseDirectory(juce::String name, Handler* handler);
};    

 
