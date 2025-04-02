 
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/TreeForm.h"
#include "../../model/StaticConfig.h"
#include "../../Provider.h"

#include "ParameterTree.h"
#include "ParameterFormCollection.h"

#include "ParameterTreeForms.h"

/**
 * From the docs for StretchableLayoutManager::setItemLayout
 *
 *    itemIndex
 *    minimumSize
 *      the minimum size that this item is allowed to be - a positive number indicates an
 *      absolute size in pixels. A negative number indicates a proportion of the available
 *      space (e.g -0.5 is 50%) 
 *    maximumSize
 *       the maximum size that this item is allowed to be - a positive number indicates
 *       an absolute size in pixels. A negative number indicates a proportion of the
 *       available space
 *    preferredSize
 *      the size that this item would like to be, if there's enough room. A positive number
 *      indicates an absolute size in pixels. A negative number indicates a proportion of
 *      the available space
 */
ParameterTreeForms::ParameterTreeForms()
{
    addAndMakeVisible(tree);
    addAndMakeVisible(forms);

    // set up the layout and resizer bars..

    // These numbers are taken from a Juce demo, I don't understand them
    // but they look good enough for the parameter tree
    
    // demo comments: "width of the font list must be between 20% and 80%, preferably 50%"
    // demo: verticalLayout.setItemLayout (0, -0.2, -0.8, -0.35);
    verticalLayout.setItemLayout (0, -0.2, -0.8, -0.20);
    
    // the vertical divider drag-bar thing is always 8 pixels wide
    verticalLayout.setItemLayout (1, 8, 8, 8);

    // demo comments:
    // "the components on the right must be at least 150 pixels wide,
    // preferably 50% of the total width"
    // demo: verticalLayout.setItemLayout (2, 150, -1.0, -0.65);
    verticalLayout.setItemLayout (2, 150, -1.0, -0.80);

    verticalDividerBar.reset (new juce::StretchableLayoutResizerBar (&verticalLayout, 1, true));
    addAndMakeVisible (verticalDividerBar.get());

    // we will be the default listener for the tree
    tree.setListener(this);
}

ParameterTreeForms::~ParameterTreeForms()
{
}

/**
 * Option to use with form collections where the same parameter
 * may appear in more than one form.  Since changing the parameter
 * in one form need to be reflected in other forms, whenever the displayed
 * form changes, it is saved and the new form is reloaded.
 */
void ParameterTreeForms::setDuplicateParameters(bool b)
{
    forms.setDuplicateParameters(b);
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
    if (formName == "none") {
        // this convention is used for a few nodes that contain other
        // categories but won't have any parameters or a form
        // it's confusing to click on it and have nothing happen so open
        // it so the user can see the subcategories
        // todo: this is the only node that auto-opens on click, perhaps they all should?
        item->setOpen(true);
    }
    else {
        if (formName.length() == 0) {
            // default form name is a combination of the tree name and node name
            // this only applies to the static forms used by the global editor
            formName = treeName + item->getName();
        }
        forms.show(formName);
    }
}

/**
 * Stupid utility to locate the TreeForm definition given an unqualified
 * form name from the tree.
 *
 * The parameterFormCollectionCreate method gets most of what it needs from
 * the SymbolTreeItem that was clicked, but to get the form title and possibly
 * other things it needs the TreeForm from the StaticConfig.
 * ParameterTree has already done that to build the ordered tree items, but
 * but it didn't save it anywhere.  Must follow the same naming convention
 * it used to find the form.  Would be better if this were remembered on the item.
 */
TreeForm* ParameterTreeForms::getTreeForm(Provider* p, juce::String formName)
{
    juce::String staticFormName = juce::String("sessionCategory") + formName;
    StaticConfig* scon = p->getStaticConfig();
    TreeForm* formdef = scon->getTreeForm(staticFormName);
    if (formdef == nullptr) {
        Trace(1, "ParameterTreeForms: Unable to locate form definition %s",
              staticFormName.toUTF8());
    }
    return formdef;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
