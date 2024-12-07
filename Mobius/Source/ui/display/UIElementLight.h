/**
 * An implementation of UIElement that displays a read-only "light"
 * of some form.  Used for seeing the status of a script variable.
 */

#include <JuceHeader.h>

#include "UIElement.h"

class UIElementLight : public UIElement
{
  public:
    
    UIElementLight(class Provider* p, class UIElementDefinition* d);
    ~UIElementLight();

    void configure() override;

    // respond to update notifications if interested
    void update(class MobiusView* view) override;

    // tell us about yourself
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    // do Jucy things
    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    juce::Colour onColor;
    juce::Colour offColor;
    juce::String monitor;

    class Symbol* symbol = nullptr;
    int lastValue = 0;
    
};

    


