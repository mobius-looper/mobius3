 
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../Provider.h"

#include "SymbolTree.h"
#include "DynamicParameterTree.h"
#include "DynamicFormCollection.h"

#include "DynamicTreeForms.h"

DynamicTreeForms::DynamicTreeForms()
{
    addAndMakeVisible(tree);
    addAndMakeVisible(forms);

    tree.setListener(this);

    // set up the layout and resizer bars..
    verticalLayout.setItemLayout (0, -0.2, -0.8, -0.35); // width of the font list must be
    // between 20% and 80%, preferably 50%
    verticalLayout.setItemLayout (1, 8, 8, 8);           // the vertical divider drag-bar thing is always 8 pixels wide
    verticalLayout.setItemLayout (2, 150, -1.0, -0.65);  // the components on the right must be
    // at least 150 pixels wide, preferably 50% of the total width

    verticalDividerBar.reset (new juce::StretchableLayoutResizerBar (&verticalLayout, 1, true));
    addAndMakeVisible (verticalDividerBar.get());
}

DynamicTreeForms::~DynamicTreeForms()
{
}

/**
 * Initialize with the full symbol table
 */
void DynamicTreeForms::initialize(Provider* p)
{
    provider = p;

    tree.initialize(p);
    
    tree.selectFirst();
}

/**
 * Load values after initialization.
 */
void DynamicTreeForms::load(ValueSet* set)
{
    valueSet = set;
    forms.load(provider, set);
}

/**
 * Initialize with a restricted value set.
 */
void DynamicTreeForms::initialize(Provider* p, ValueSet* set)
{
    provider = p;
    valueSet = set;

    tree.initialize(p, set);
    forms.load(p, set);
    
    tree.selectFirst();
}

void DynamicTreeForms::decache()
{
    forms.decache();
}

void DynamicTreeForms::save()
{
    save(valueSet);
}

void DynamicTreeForms::save(class ValueSet* set)
{
    forms.save(set);
}

void DynamicTreeForms::cancel()
{
    forms.cancel();
}

void DynamicTreeForms::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    bool withDivider = true;
    if (withDivider) {

        // lay out the list box and vertical divider..
        Component* vcomps[] = { &tree, verticalDividerBar.get(), nullptr };

        verticalLayout.layOutComponents (vcomps, 3,
                                         area.getX(), area.getY(), area.getWidth(), area.getHeight(),
                                         false,     // lay out side-by-side
                                         true);     // resize the components' heights as well as widths


        area.removeFromLeft (verticalDividerBar->getRight());
        forms.setBounds(area);
     }
    else {
        tree.setBounds(area.removeFromLeft(100));
        forms.setBounds(area);
    }
}

void DynamicTreeForms::symbolTreeClicked(SymbolTreeItem* item)
{
    SymbolTreeItem* container = item;
    
    // if this is a leaf node, go up to the parent and show the entire parent form
    if (item->getNumSubItems() == 0) {
        container = static_cast<SymbolTreeItem*>(item->getParentItem());
    }

    // SymbolTreeItem is a generic model that doesn't understand it's usage
    // for form delivery, the tree builder left the form name as the "annotation"
    
    juce::String formName = container->getAnnotation();
    if (formName.length() > 0) {

        ParameterForm* form = forms.getForm(formName);
        if (form == nullptr) {
            form = buildForm(container);
            forms.addForm(formName, form);
        }
        forms.show(provider, formName);
    }
}

ParameterForm* DynamicTreeForms::buildForm(SymbolTreeItem* parent)
{
    ParameterForm* form = new ParameterForm();

    for (int i = 0 ; i < parent->getNumSubItems() ; i++) {
        SymbolTreeItem* item = static_cast<SymbolTreeItem*>(parent->getSubItem(i));
        form->add(item->getSymbols());
    }
    
    return form;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
