
#include <JuceHeader.h>

#include "model/SystemConfig.h"
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

        // !! actually, I don't like defaulting to the installation root
        // because almost nothing of interesting is going to be there
        // for user files
        // ugh, this needs to be in the released version but it's too painful
        // for development to have to walk back here
        //juce::File home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        //path = home.getFullPathName();
    }
    return path;
}

void Pathfinder::saveLastFolder(juce::String purpose, juce::String path)
{
    lastFolders.set(purpose, path);
}

void Pathfinder::load(SystemConfig* scon)
{
    (void)scon;
}

bool Pathfinder::save(SystemConfig* scon)
{
    bool changed = false;
    
    (void)scon;
    
    return changed;
}
