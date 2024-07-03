/**
 * Utility class to save and load Mobius Project structures.
 *
 * This is mostly old code reorganized to separate Project model handling
 * from file handling.  The Project class defines the memory model for a project and
 * the code to extract and install it within the engine.
 *
 * ProjectManager is responsible for reading and writing Project objects to the
 * file system and interacting with the user.
 *
 * One of these will be maintained within MobiusShell.
 *
 */

#pragma once

class ProjectManager
{
  public:
    
    ProjectManager(class MobiusShell* shell);
    ~ProjectManager();

    juce::StringArray save(juce::File dest);
    juce::StringArray load(juce::File src);

  private:

    class MobiusShell* shell;
    juce::StringArray errors;
    
};
