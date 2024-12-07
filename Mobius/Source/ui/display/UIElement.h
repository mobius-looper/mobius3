/**
 * The base component for things that can be displayed
 * in the main display area and in the track strips.
 *
 * This is an evolution of some existing concepts and will
 * eventually replace them:
 *
 *     DisplayElement, StripElement, StripElementDefinition, etc.
 *
 *
 * UIElements share a few common characteristics.
 *
 *     - they are juce::Components and can be organized as such
 *       in any Component tree
 *
 *     - they are associated with a UIElementDefinition that contains
 *       user configurable settings for how they are rendered and what they do
 *
 *     - they are referenced by larger display organization components
 *       through a UIElementRef
 *
 *     - they receive configure() notifications when configuration settings change
 *     - they receive periodic update() notifications that they may use for monitoring
 *       something to display
 *
 */

#pragma once

#include <JuceHeader.h>

class UIElement : public juce::Component
{
  public:

    // elements must be created with a Provider and a definition
    UIElement(class Provider* p, class UIElementDefinition* def);
    virtual ~UIElement();

    // respond to configuration changes if interested
    virtual void configure();

    // respond to update notifications if interested
    virtual void update(class MobiusView* view);

    // tell us about yourself
    virtual int getPreferredWidth();
    virtual int getPreferredHeight();

    // do Jucy things
    virtual void resized() override;
    virtual void paint(juce::Graphics& g) override;

  private:

    class Provider* provider = nullptr;
    class UIElementDefinition definition = nullptr;
    
};


