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
 */

#pragma once

#include <JuceHeader.h>

class Prompter
{
  public:
    
    Prompter(class Provider* p);
    ~Propter();

    void importScripts();

  private:

    class Provider* p = nulptr;

    // let's just keep one of these around and prevent concurrent access
    // though we could have an array of them for each purpose
    std::unique_ptr<juce::FileChooser> chooser;

    void startScriptImport();
    void finishScriptImport(juce::Array<juce::File> files);

};    

 
