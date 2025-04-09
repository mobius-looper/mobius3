/**
 * Small utility managed by Supervisor to locate where things
 * go and remember where users went.
 *
 * This started as a way to remember the last path used in
 * variouis FileBrowserComponents so we have a common place
 * to remember those, and things like ScriptInteractor can be
 * transient objects that don't need any long-duration state.
 *
 * Now that we have this, it could become more of a hub for other
 * file-related needs, perhaps replacing RootLocator or managing
 * the user-specified folder locations for the script library.
 */

#pragma once

#include <JuceHeader.h>

class Pathfinder
{
  public:

    Pathfinder(class Provider* p);
    ~Pathfinder();

    /**
     * Locate the last folder used by a browser for a given purpose.
     */
    juce::String getLastFolder(juce::String purpose);

    void saveLastFolder(juce::String purpose, juce::String folder);

    /**
     * Load previous used paths saved in the system config.
     */
    void load(class SystemConfig* config);

    /**
     * Save the paths used in this runtime session to the system config.
     */
    bool save(class SystemConfig* config);

  private:

    class Provider* provider = nullptr;
    juce::HashMap<juce::String,juce::String> lastFolders;

};
