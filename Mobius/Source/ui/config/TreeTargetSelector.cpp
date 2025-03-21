
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/List.h"

#include "../../model/old/ActionType.h"
#include "../../model/old/MobiusConfig.h"
#include "../../model/old/Preset.h"
#include "../../model/old/Setup.h"
#include "../../model/old/OldBinding.h"
#include "../../model/Symbol.h"
#include "../../model/UIConfig.h"
#include "../../Supervisor.h"

#include "OldBindingTargetSelector.h"

#include "TreeTargetSelector.h"

TreeTargetSelector::TreeTargetSelector(Supervisor* s)
{
    supervisor = s;
    addAndMakeVisible(tree);
}

TreeTargetSelector::~TreeTargetSelector()
{
}

void TreeTargetSelector::load()
{
    UIConfig* config = supervisor->getUIConfig();
    juce::String favorites = config->get("symbolTreeFavorites");
    tree.loadSymbols(supervisor->getSymbols(), favorites);
}

void TreeTargetSelector::save()
{
    UIConfig* config = supervisor->getUIConfig();
    config->put("symbolTreeFavorites", tree.getFavorites());
}

void TreeTargetSelector::reset()
{
}

void TreeTargetSelector::select(class OldBinding* b)
{
    (void)b;
}

void TreeTargetSelector::capture(class OldBinding* b)
{
    (void)b;
}

bool TreeTargetSelector::isTargetSelected()
{
    return false;
}

void TreeTargetSelector::resized()
{
    tree.setBounds(getLocalBounds());
}

