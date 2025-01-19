
#include <JuceHeader.h>

#include "../../util/Trace.h"
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

    tracks->setListener(this);
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

/**
 * This is called when the selected row changes either by clicking on
 * it or using the keyboard arrow keys after a row has been selected.
 */
void SessionTrackEditor::typicalTableChanged(TypicalTable* t, int row)
{
    (void)t;
    //Trace(2, "SessionTrackEditor: Table changed row %d", row);

    if (tracks->isMidi(row)) {
        trees->showMidi(true);
    }
    else {
        trees->showMidi(false);
    }
}
