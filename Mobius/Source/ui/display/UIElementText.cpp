/**
 * Some options for text
 *
 *   1) Simple text with different on/off colors
 *   2) Alternating text with different words for on/off
 *   3) Word pairs with one or the other highlighted
 *   4) Work sequence with value selecting word
 *
 */
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/UIConfig.h"
#include "../../model/Symbol.h"
#include "../../model/Query.h"
#include "../../Provider.h"

#include "UIElement.h"
#include "UIElementText.h"

UIElementText::UIElementText(Provider* p, UIElementDefinition* d) :
    UIElement(p, d)
{
    monitor = d->properties["monitor"];
    text = d->properties["text"];
    width = d->properties["width"].getIntValue();
    height = d->properties["height"].getIntValue();
    onColor = UIElement::getColor(d, "onColor");
    offColor = UIElement::getColor(d, "offColor");

    if (monitor.length() == 0)
      Trace(1, "UIElementText: Missing monitor variable name");
    else {
        // todo: might be nice to be able to query on things that aren't
        // exported, kind of in beteen static variables that don't need
        // full blown Symbols
        symbol = p->getSymbols()->find(monitor);
        if (symbol == nullptr)
          Trace(1, "UIElementText: Invalid symbol name %s", monitor.toUTF8());
    }
}

UIElementText::~UIElementText()
{
}

void UIElementText::configure()
{
}

int UIElementText::getPreferredWidth()
{
    // be smart about text width
    return (width > 0) ? width : 30;
}

int UIElementText::getPreferredHeight()
{
    return (height > 0) ? height : 14;
}

void UIElementText::update(class MobiusView* v)
{
    (void)v;
    if (symbol != nullptr) {
        Query q;
        q.symbol = symbol;
        q.scope = scope;
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

void UIElementText::resized()
{
}

void UIElementText::paint(juce::Graphics& g)
{
    if (lastValue == 0)
      g.setColour(offColor);
    else
      g.setColour(onColor);

    g.drawText(text, 0, 0, getWidth(), getHeight(), juce::Justification::centred);
}
