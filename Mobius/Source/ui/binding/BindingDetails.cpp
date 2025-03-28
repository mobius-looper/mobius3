
#include <JuceHeader.h>

#include "../JuceUtil.h"
#include "BindingDetails.h"

BindingDetailsPanel::BindingDetailsPanel()
{
    // don't really need a title on these
    // but without a title bar you don't get mouse
    // events for dragging
    //setTitle("Binding");
    setContent(&content);
    // this gives it a yellow border
    setAlert();
    // this gives it dragability within the entire window since
    // these don't have a title bar
    followContentMouse();
        
    setSize(700, 500);
}

void BindingDetailsPanel::show(juce::Component* parent)
{
    // since Juce can't seem to control z-order, even if we already have this parent
    // (which is unlikely), remove and add it so it's at the top
    juce::Component* current = getParentComponent();
    if (current != nullptr)
      current->removeChildComponent(this);
    parent->addAndMakeVisible(this);
    
    // why was this necessary?
    content.resized();
    JuceUtil::centerInParent(this);
    BasePanel::show();
}

void BindingDetailsPanel::close()
{
    juce::Component* current = getParentComponent();
    if (current != nullptr) {
        current->removeChildComponent(this);
    }
}

void BindingDetailsPanel::initialize(Provider* p)
{
    content.initialize(p);
}

//////////////////////////////////////////////////////////////////////
//
// Content
//
//////////////////////////////////////////////////////////////////////

BindingContent::BindingContent()
{
    addAndMakeVisible(tree);
}

void BindingContent::initialize(Provider* p)
{
    tree.initialize(p);
}

void BindingContent::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    tree.setBounds(area.removeFromLeft(300));
}



/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
