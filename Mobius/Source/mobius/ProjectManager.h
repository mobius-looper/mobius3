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
 * This is EXTREMELY hacked up, just trying to make it work like it did before.
 * once the dust settles, it will be entirely replaced.
 *
 */

#pragma once

class ProjectManager
{
  public:
    
    ProjectManager(class MobiusShell* shell);
    ~ProjectManager();

    juce::StringArray saveProject(juce::File dest);
    juce::StringArray loadProject(juce::File src);

    juce::StringArray loadLoop(juce::File file);
    juce::StringArray saveLoop(juce::File file);
    
  private:

    class MobiusShell* shell;
    juce::StringArray errors;
    juce::File projectRoot;

    // ancient file management
    class Project* saveProject();
    void writeProject(class Project* p, const char* file, bool isTemplate);
    void writeAudio(class Project* p, const char* baseName);
    void writeAudio(class ProjectTrack* track, const char* baseName, int tracknum);
    void writeAudio(class ProjectLoop* loop, const char* baseName, int tracknum, int loopnum);
    void writeAudio(class ProjectLayer* layer, const char* baseName,
                    int tracknum, int loopnum, int layernum);
    void writeAudio(class Audio* audio, const char* path);
    void deleteAudioFiles(class Project* p);

    //bool EndsWithNoCase(const char* str, const char* suffix);
    //int LastIndexOf(const char* str, const char* substr);
    int WriteFile(const char* name, const char* content);

    void read(Project* p, juce::File file);
    void readAudio(Project* p);
    Audio* readAudio(const char* path);
    
    bool looksAbsolute(juce::String path);
};
