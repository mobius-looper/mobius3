/**
 * Temporary encapsulation of a few utilities to get project save/load started.
 *
 * Not at all happy with the layering right now, most of the work is done
 * in MobiusShell and all we do out here is present the file chooser for the
 * source and destination paths.
 *
 * As this evolves there will be overlap between this and AudioClerk and
 * eventually whatever happens with "sessions".
 */

#include <JuceHeader.h>

#pragma once

class ProjectFiler
{
  public:
    
    ProjectFiler(class Supervisor* s) {
        supervisor = s;
    }

    ~ProjectFiler() {
    }

    void loadProject();
    void saveProject();
    void loadLoop();
    void loadLoop(int trackNumber, int loopNumber);
    void saveLoop();
    void saveLoop(int trackNumber, int loopNumber);
    void dragOut(int trackNumber, int loopNumber);
    void quickSave();
    
  private:

    class Supervisor* supervisor = nullptr;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastFolder;
    int destinationTrack = 0;
    int destinationLoop = 0;
    
    void chooseProjectSave();
    void doProjectSave(juce::File file);
    
    void chooseProjectLoad();
    void doProjectLoad(juce::File file);

    void chooseLoopSave();
    void doLoopSave(juce::File file);
    
    void chooseLoopLoad();
    void doLoopLoad(juce::File file);
    
    juce::File uniqueify(juce::File src);

    void showErrors(juce::StringArray& errors);
    
};

        
