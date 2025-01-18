
#include <JuceHeader.h>

#include "../../model/Session.h"
#include "../../Provider.h"
#include "../JuceUtil.h"

#include "SessionEditor.h"
#include "SessionTrackTable.h"
#include "SessionTrackTrees.h"

#include "SessionTrackEditor.h"

SessionTrackEditor::SessionTrackEditor(Provider* p)
{
    provider = p;
    tracks.reset(new SessionTrackTable(p));
    trees.reset(new SessionTrackTrees());
    
    addAndMakeVisible(tracks.get());
    addAndMakeVisible(trees.get());
}

SessionTrackEditor::~SessionTrackEditor()
{
}

void SessionTrackEditor::loadSymbols()
{
    trees->load(provider);
}

void SessionTrackEditor::load()
{
    tracks->load();
}

void SessionTrackEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    tracks->setBounds(area.removeFromLeft(200));
    trees->setBounds(area.removeFromLeft(400));

    JuceUtil::dumpComponent(this);
}
