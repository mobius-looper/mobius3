 
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
