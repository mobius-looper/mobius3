
#include <JuceHeader.h>

#include "StatusElement.h"
#include "StatusResizer.h"

StatusResizer::StatusResizer(StatusElement* el) :
    juce::ResizableBorderComponent(el, nullptr)
{
    element = el;
}

void StatusResizer::mouseEnter(const juce::MouseEvent& event)
{
    // this is enough to get the border drawn
    element->mouseEnter(event);
    juce::ResizableBorderComponent::mouseEnter(event);
}

void StatusResizer::mouseExit(const juce::MouseEvent& event)
{
    element->mouseExit(event);
    juce::ResizableBorderComponent::mouseExit(event);
}


