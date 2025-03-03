/**
 * Besides a few basic group tools, the main thing this does is hide where GroupDefinitions
 * come from to make it easire to move them out of MobiusConfig.
 *
 */

#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/old/MobiusConfig.h"
#include "model/GroupDefinition.h"

#include "Provider.h"

#include "Grouper.h"

Grouper::Grouper(Provider* p)
{
    provider = p;
}

Grouper::~Grouper()
{
}

void Grouper::getGroupNames(juce::StringArray& names)
{
    MobiusConfig* mc = provider->getOldMobiusConfig();
    for (int i = 0 ; i < mc->dangerousGroups.size() ; i++) {
        GroupDefinition* def = mc->dangerousGroups[i];
        names.add(def->name);
    }
}

int Grouper::getGroupOrdinal(juce::String name)
{
    MobiusConfig* mc = provider->getOldMobiusConfig();
    return mc->getGroupOrdinal(name);
}
