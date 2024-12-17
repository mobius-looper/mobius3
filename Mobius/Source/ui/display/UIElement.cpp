/**
 * Base component for UI elements.
 *
 * Doesn't do much, but it's there for you if you need it.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/UIConfig.h"

#include "UIElement.h"
#include "UIElementLight.h"
#include "UIElementText.h"

UIElement::UIElement(Provider* p, UIElementDefinition* d)
{
    // don't need to pass this if we can't find a reason for it
    (void)d;
    provider = p;
}

UIElement::~UIElement()
{
}

void UIElement::configure()
{
}

void UIElement::update(class MobiusView* v)
{
    (void)v;
}

int UIElement::getPreferredWidth()
{
    return 20;
}

int UIElement::getPreferredHeight()
{
    return 20;
}


void UIElement::resized()
{
}

// not intended to be called
void UIElement::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::blue);
    g.fillRect(0, 0, getWidth(), getHeight());
}

//////////////////////////////////////////////////////////////////////
//
// Mouse Forwarding
//
// We have to forward mouse events to the parent which is the
// StatusElement or StripElement that implements mouse sensitivity.
// Alternately, we could try implementing both superclasses but
// it gets messy.
//
//////////////////////////////////////////////////////////////////////

void UIElement::mouseEnter(const juce::MouseEvent& event)
{
    getParentComponent()->mouseEnter(event);
}

void UIElement::mouseExit(const juce::MouseEvent& event)
{
    getParentComponent()->mouseExit(event);
}

void UIElement::mouseDown(const juce::MouseEvent& event)
{
    getParentComponent()->mouseDown(event);
}

void UIElement::mouseDrag(const juce::MouseEvent& event)
{
    getParentComponent()->mouseDrag(event);
}

void UIElement::mouseUp(const juce::MouseEvent& e)
{
    getParentComponent()->mouseUp(e);
}

//////////////////////////////////////////////////////////////////////
//
// Factory
//
//////////////////////////////////////////////////////////////////////

/**
 * Create an appropriate UIElement object to implement the visualization
 * defined in the UIElementDefinition.
 */
UIElement* UIElement::createElement(Provider* p, UIElementDefinition* def)
{
    UIElement* element = nullptr;

    if (def->visualizer == "Light") {
        element = new UIElementLight(p, def);
    }
    else if (def->visualizer == "Text") {
        element = new UIElementText(p, def);
    }
    else {
        Trace(1, "UIElement: Unknown element visualizer %s", def->visualizer.toUTF8());
    }
    return element;
}

//////////////////////////////////////////////////////////////////////
//
// Tools
//
//////////////////////////////////////////////////////////////////////

juce::Colour UIElement::getColor(UIElementDefinition* def, juce::String usage)
{
    juce::Colour c = juce::Colours::white;
    juce::String name = def->properties[usage];
    if (name.length() == 0) {
        Trace(1, "UIElement: Missing color name for usage %s, defaulting to white",
              usage.toUTF8());
    }
    else {
        c = getColor(name);
    }
    return c;
}
    
juce::Colour UIElement::getColor(juce::String name)
{
    juce::Colour c = juce::Colours::findColourForName(name, juce::Colours::white);
    if (c == juce::Colours::white) {
        // the default should be used only if they actually asked for white
        if (name != "white")
          Trace(1, "UIElement: Invalid color name %s, defaulting to white",
                name.toUTF8());
    }
    return c;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
