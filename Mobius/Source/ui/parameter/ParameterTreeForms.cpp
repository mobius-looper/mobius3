 
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "ParameterTree.h"
#include "ParameterFormCollection.h"

#include "ParameterTreeForms.h"

ParameterTreeForms::ParameterTreeForms()
{
    addAndMakeVisible(tree);
    addAndMakeVisible(forms);

    // set up the layout and resizer bars..
    verticalLayout.setItemLayout (0, -0.2, -0.8, -0.35); // width of the font list must be
    // between 20% and 80%, preferably 50%
    verticalLayout.setItemLayout (1, 8, 8, 8);           // the vertical divider drag-bar thing is always 8 pixels wide
    verticalLayout.setItemLayout (2, 150, -1.0, -0.65);  // the components on the right must be
    // at least 150 pixels wide, preferably 50% of the total width

    verticalDividerBar.reset (new juce::StretchableLayoutResizerBar (&verticalLayout, 1, true));
    addAndMakeVisible (verticalDividerBar.get());

    // we will be the default listener for the tree
    tree.setListener(this);
}

ParameterTreeForms::~ParameterTreeForms()
{
}

void ParameterTreeForms::resized()
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

/**
 * When a tree node is clicked, ask the form collection to display the form.
 * The collection may call back to the Factory to ask for the form.
 */
void ParameterTreeForms::symbolTreeClicked(SymbolTreeItem* item)
{
    SymbolTreeItem* container = item;
    
    // if this is a leaf node, go up to the parent and show the entire parent form
    if (item->getNumSubItems() == 0) {
        container = static_cast<SymbolTreeItem*>(item->getParentItem());
    }

    // SymbolTreeItem is a generic model that doesn't understand it's usage
    // by convention for parameter editors, the tree builder left the form name
    // as the "annotation"
    
    juce::String formName = container->getAnnotation();
    if (formName.length() == 0) {
        // default form name is a combination of the tree name and node name
        // this only applies to the static forms used by the global editor
        formName = treeName + item->getName();
    }
    
    forms.show(formName);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
