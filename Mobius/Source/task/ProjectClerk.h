/**
 * Utility to manage exporting and importing projects.
 *
 * Eventual replacement for the older ProjectFiler
 *
 */

#pragma once

#include <JuceHeader.h>

class ProjectClerk
{
  public:

    ProjectClerk(class Provider* p);
    ~ProjectClerk();
    
    int writeProject(class Task* task, juce::File folder, class TrackContent* content);

    class TrackContent* readSnapshot(class Task* task, juce::File file);
    class TrackContent* readOld(class Task* task, juce::File file);

  private:

    class Provider* provider = nullptr;

    void writeProject(class Task* t);
    void cleanFolder(class Task* t, juce::File folder);
    void cleanFolder(class Task* t, juce::File folder, juce::String extension);

};
