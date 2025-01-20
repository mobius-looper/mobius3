 
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../Provider.h"

#include "SymbolTree.h"
#include "SessionEditorTree.h"
#include "SessionFormCollection.h"

#include "SessionTreeForms.h"

SessionTreeForms::SessionTreeForms()
{
    addAndMakeVisible(tree);
    addAndMakeVisible(forms);

    tree.setListener(this);
}

SessionTreeForms::~SessionTreeForms()
{
}

void SessionTreeForms::initialize(Provider* p, juce::String treeName)
{
    provider = p;
    tree.load(p, treeName);
}

void SessionTreeForms::load(ValueSet* src)
{
    forms.load(src);
}

void SessionTreeForms::save(ValueSet* dest)
{
    forms.save(dest);
}

void SessionTreeForms::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    tree.setBounds(area.removeFromLeft(200));
    forms.setBounds(area);
}

void SessionTreeForms::symbolTreeClicked(SymbolTreeItem* item)
{
    SymbolTreeItem* container = item;
    
    // if this is a leaf node, go up to the parent and show the entire parent form
    if (item->getNumSubItems() == 0) {
        container = static_cast<SymbolTreeItem*>(item->getParentItem());
    }

    // SymbolTreeItem is a generic model that doesn't understand it's usage
    // for form delivery, the tree builder left the form name as the "annotation"
    
    juce::String formName = container->getAnnotation();
    if (formName.length() == 0) {
        // okay for interior nodes, but test
        Trace(2, "SessionTreeForms: No form for node %s", item->getName().toUTF8());
    }
    else {
        forms.show(provider, formName);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
