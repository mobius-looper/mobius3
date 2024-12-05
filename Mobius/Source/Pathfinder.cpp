
#include <JuceHeader.h>

#include "Provider.h"

#include "Pathfinder.h"

Pathfinder::Pathfinder(Provider* p)
{
    provider = p;
}

Pathfinder::~Pathfinder()
{
}

juce::String Pathfinder::getLastFolder(juce::String purpose)
{
    juce::String path = lastFolders[purpose];
    if (path.length() == 0) {
        // todo, should have some defaults for each purpose
        juce::File root = provider->getRoot();
        // maybe pathfinder should deal with File rather than String
        path = root.getFullPathName();
    }
    return path;
}

void Pathfinder::saveLastFolder(juce::String purpose, juce::String path)
{
    lastFolders.set(purpose, path);
}
