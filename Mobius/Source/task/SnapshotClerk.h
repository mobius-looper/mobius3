/**
 * Utility to manage exporting and importing snapshots.
 *
 * Also conversion of old Projects into Snapshots.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "../mobius/TrackContent.h"

class SnapshotClerk
{
  public:

    SnapshotClerk(class Provider* p);
    ~SnapshotClerk();
    
    int writeSnapshot(class Task* task, juce::File folder, class TrackContent* content);

    class TrackContent* readSnapshot(class Task* task, juce::File file);
    class TrackContent* readProject(class Task* task, juce::File file);

  private:

    class Provider* provider = nullptr;

    void writeSnapshot(class Task* t);
    void cleanFolder(class Task* t, juce::File folder);
    void cleanFolder(class Task* t, juce::File folder, juce::String extension);

    // old .mob file reading
    void parseOldTrack(class Task* task, juce::File project, TrackContent* content, juce::XmlElement* root);
    void parseOldLoop(class Task* task, juce::File project, TrackContent::Track* track, juce::XmlElement* root);
    void parseOldLayer(class Task* task, juce::File project, TrackContent::Loop* loop, juce::XmlElement* root);
    bool looksAbsolute(juce::String path);

    // mcl parse state
    int mclLineNumber = 0;
    bool mclSectionFound = false;
    TrackContent::Track* mclTrack = nullptr;
    TrackContent::Loop* mclLoop = nullptr;
    TrackContent::Layer* mclLayer = nullptr;
    
    void parseSnapshotMcl(class Task* task, juce::File root, juce::String src, TrackContent* content);
    int readArgument(Task* task, juce::StringArray& tokens, juce::String keyword);
    void ensureLayer();
    class Audio* readAudio(class Task* task, juce::File f);
    class MidiSequence* readMidi(class Task* task, juce::File f);
    void validateContent(TrackContent* content);
    
};
