 
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

void SessionTreeForms::initialize(Provider* p, juce::String argTreeName)
{
    provider = p;
    treeName = argTreeName;
    tree.load(p, treeName);
}

void SessionTreeForms::decache()
{
    forms.decache();
}

void SessionTreeForms::load(ValueSet* src)
{
    forms.load(src);
}

void SessionTreeForms::save(ValueSet* dest)
{
    forms.save(dest);
}

void SessionTreeForms::cancel()
{
    forms.cancel();
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
        // default form name is a combination of the tree name and node name
        formName = treeName + item->getName();
    }
    
    forms.show(provider, formName);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
