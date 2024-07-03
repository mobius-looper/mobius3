/**
 * Utility class to organize the project loading and saving process.
 *
 * Not happy with the layering here, but this gets it going.
 * What I'd like is to have MobiusInterface receive and return standalone
 * Project objects and push all file handling up to the UI where we can play around
 * with how these are packaged since project files and the emergine "session"
 * concept are going to be closely related.
 *
 * The main sticking point is the Audio objects which we have right now
 * for loadLoop.  These like to use an AudioPool embedded deep within the
 * engine so it's easier if we deal with the model in the shell.
 *
 * Once Audio/AudioPool get redesigned it will be cleaner.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"

#include "MobiusShell.h"
#include "ProjectManager.h"

ProjectManager::ProjectManager(MobiusShell* parent)
{
    shell = parent;
}

ProjectManager::~ProjectManager()
{
}

/**
 * Main entry point to save projects.
 */
juce::StringArray ProjectManager::save(juce::File file)
{
    errors.clear();

    // magic
    Trace(2, "ProjectManager::save got all the way here with %s",
          file.getFullPathName().toUTF8());

    return errors;
}

juce::StringArray ProjectManager::load(juce::File file)
{
    errors.clear();

    // magic
    Trace(2, "ProjectManager::load got all the way here with %s",
          file.getFullPathName().toUTF8());

    return errors;
}
