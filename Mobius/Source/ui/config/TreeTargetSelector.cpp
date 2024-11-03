
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/List.h"

#include "../../model/ActionType.h"
#include "../../model/UIParameter.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Preset.h"
#include "../../model/Setup.h"
#include "../../model/Binding.h"
#include "../../model/Symbol.h"
#include "../../model/UIConfig.h"
#include "../../Supervisor.h"

#include "BindingTargetSelector.h"

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

void TreeTargetSelector::select(class Binding* b)
{
    (void)b;
}

void TreeTargetSelector::capture(class Binding* b)
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

