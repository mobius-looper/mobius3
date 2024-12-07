
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/UIConfig.h"
#include "../../model/Symbol.h"
#include "../../model/Query.h"
#include "../../Provider.h"

#include "UIElement.h"
#include "UIElementLight.h"

UIElementLight::UIElementLight(Provider* p, UIElementDefinition* d) :
    UIElement(p, d)
{
    monitor = d->properties["monitor"];
    onColor = UIElement::getColor(d, "onColor");
    offColor = UIElement::getColor(d, "offColor");

    if (monitor.length() == 0)
      Trace(1, "UIElementLight: Missing monitor variable name");
    else {
        // todo: might be nice to be able to query on things that aren't
        // exported, kind of in beteen static variables that don't need
        // full blown Symbols
        symbol = p->getSymbols()->find(monitor);
        if (symbol == nullptr)
          Trace(1, "UIElementLight: Invalid symbol name %s", monitor.toUTF8());
    }
}

UIElementLight::~UIElementLight()
{
}

void UIElementLight::configure()
{
}

int UIElementLight::getPreferredWidth()
{
    return 20;
}

int UIElementLight::getPreferredHeight()
{
    return 20;
}

void UIElementLight::update(class MobiusView* v)
{
    (void)v;
    if (symbol != nullptr) {
        Query q;
        q.symbol = symbol;
        // todo: this element will either have track scope or
        // use the focused track
        if (provider->doQuery(&q)) {
            // todo: need to support string values with Query somehow,
            // or maybe just skip Query and assume these are always MSL variables?
            if (q.value != lastValue) {
                lastValue = q.value;
                repaint();
            }
        }
    }
}

void UIElementLight::resized()
{
}

void UIElementLight::paint(juce::Graphics& g)
{
    if (lastValue == 0)
      g.setColour(offColor);
    else
      g.setColour(onColor);
    g.fillRect(0, 0, getWidth(), getHeight());
}
