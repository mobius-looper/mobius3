/**
 * Utility to manage exporting and importing projects.
 *
 * Eventual replacement for the older ProjectFiler
 *
 */

#pragma once

#include <JuceHeader.h>

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
        File() {};
        ~File();
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

class ProjectClerk
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

  private:

    class Provider* provider = nullptr;

};
