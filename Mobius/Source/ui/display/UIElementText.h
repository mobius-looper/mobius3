/**
 * An implementation of UIElement that displays a read-only text
 * with color highlighting.
 */

#include <JuceHeader.h>

#include "UIElement.h"

class UIElementText : public UIElement
{
  public:
    
    UIElementText(class Provider* p, class UIElementDefinition* d);
    ~UIElementText();

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

    juce::String text;
    int width = 0;
    int height = 0;
    juce::Colour onColor;
    juce::Colour offColor;
    juce::String monitor;

    class Symbol* symbol = nullptr;
    int lastValue = 0;
    
};

    


