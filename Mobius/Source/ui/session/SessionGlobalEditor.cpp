/**
 * SessionEditor subcomponent for editing the global sesison parameters.
 *
 * This combines a SessionEditorTree for the global parameter categories
 * and a SessionFormCollection for categories in the tree.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/ValueSet.h"
#include "../../Provider.h"

#include "SessionEditorTree.h"
#include "SessionFormCollection.h"

#include "SessionGlobalEditor.h"

SessionGlobalEditor::SessionGlobalEditor()
{
    addAndMakeVisible(&tree);
    addAndMakeVisible(&forms);

    tree.setListener(this);
}

SessionGlobalEditor::initialize(Provider* p)
{
    provider = p;
    tree.load(p->getSymbols(), "sessionGlobal");
}

void SessionGlobalEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    int half = getHeight() / 2;
    tree.setBounds(area.removeFromLeft(half));
    forms.setBounds(area);
}

void SessionGlobalEditor::load(ValueSet* src)
{
    forms.load(src);
}

void SessionGlobalEditor::save(Session* dest)
{
    forms.save(dest);
}

void SessionGlobalEditor::symbolTreeClicked(SymbolTreeItem* item)
{
    SymbolTreeItem* container = item;
    
    // if this is a leaf node, go up to the parent and show the entire parent form
    if (item->getNumSubItems() == 0) {
        container = static_cast<SymbolTreeItem*>(item->getParentItem());
    }

    juce::String formName = container->getAnnotation();
    if (formName.length() == 0) {
        // okay for interior nodes, but test
        Trace(2, "SessionGlobalEditor: No form for node %s", item->getName().toUTF8());
    }
    else {
        forms.show(provider, formName);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
