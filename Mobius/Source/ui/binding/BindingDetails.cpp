
#include <JuceHeader.h>

#include "../JuceUtil.h"
#include "BindingDetails.h"

BindingDetailsPanel::BindingDetailsPanel()
{
    // don't really need a title on these
    // but without a title bar you don't get mouse
    // events for dragging, unless you use followContentMouse
    //setTitle("Binding");

    setContent(&content);

    // this gives it dragability within the entire window since
    // these don't have a title bar
    followContentMouse();

    // this gives it a yellow border
    setAlert();

    resetButtons();
    addButton(&saveButton);
    addButton(&cancelButton);
        
    setSize(350,400);
}

void BindingDetailsPanel::show(juce::Component* parent, Binding* b)
{
    // since Juce can't seem to control z-order, even if we already have this parent
    // (which is unlikely), remove and add it so it's at the top
    juce::Component* current = getParentComponent();
    if (current != nullptr)
      current->removeChildComponent(this);
    parent->addAndMakeVisible(this);

    content.load(b);
    
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

void BindingDetailsPanel::footerButton(juce::Button* b)
{
    if (b == &saveButton) {
        content.save();
    }
    else if (b == &cancelButton) {
        content.cancel();
    }
    
    if (b == &saveButton || b == &cancelButton)
      close();
}

//////////////////////////////////////////////////////////////////////
//
// Content
//
//////////////////////////////////////////////////////////////////////

BindingContent::BindingContent()
{
    //addAndMakeVisible(tree);
    addAndMakeVisible(forms);
}

void BindingContent::initialize(Provider* p)
{
    provider = p;
    //tree.initialize(p);
}

void BindingContent::load(Binding* b)
{
    binding = b;
    forms.load(provider, b);
}

void BindingContent::save()
{
    forms.save(binding);
}

void BindingContent::cancel()
{
}

void BindingContent::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    //tree.setBounds(area.removeFromLeft(300));
    forms.setBounds(area);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
