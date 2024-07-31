/**
 * Object maintaining information about all the scripts that have been loaded
 * from the library folder and all external files.
 */

#pragma once

#include <JuceHeader.h>
#include "ScriptFile.h"

class ScriptLibrary
{
  public:

    ScriptLibrary();
    ~ScriptLibrary();

    // files in the library
    juce::OwnedArray<class ScriptFile> libraryFiles;

    // external files
    juce::OwnedArray<class ScriptFile> externalFiles;

};

