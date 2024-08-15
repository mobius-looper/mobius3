/**
 * Not much to this yet...
 */

#include <JuceHeader.h>

#include "GroupDefinition.h"

GroupDefinition::GroupDefinition()
{
}

GroupDefinition::GroupDefinition(GroupDefinition* src)
{
    name = src->name;
    color = src->color;
    replicationEnabled = src->replicationEnabled;
    replicatedFunctions = src->replicatedFunctions;
}

GroupDefinition::~GroupDefinition()
{
}

