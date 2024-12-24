
#include <JuceHeader.h>

#include "../JuceUtil.h"
#include "UIAtom.h"

//////////////////////////////////////////////////////////////////////
//
// Base Atom
//
//////////////////////////////////////////////////////////////////////

UIAtom::UIAtom()
{
}

UIAtom::~UIAtom()
{
}

int UIAtom::getPreferredWidth()
{
    return 20;
}

int UIAtom::getPreferredHeight()
{
    return 20;
}

void UIAtom::resized()
{
}

void UIAtom::paint(juce::Graphics& g)
{
    // draw something so we know it's there but the subclass
    // is supposed to overload this
    g.setColour(juce::Colours::yellow);
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

void UIAtom::mouseEnter(const juce::MouseEvent& event)
{
    getParentComponent()->mouseEnter(event);
}

void UIAtom::mouseExit(const juce::MouseEvent& event)
{
    getParentComponent()->mouseExit(event);
}

void UIAtom::mouseDown(const juce::MouseEvent& event)
{
    getParentComponent()->mouseDown(event);
}

void UIAtom::mouseDrag(const juce::MouseEvent& event)
{
    getParentComponent()->mouseDrag(event);
}

void UIAtom::mouseUp(const juce::MouseEvent& e)
{
    getParentComponent()->mouseUp(e);
}

//////////////////////////////////////////////////////////////////////
//
// Light
//
//////////////////////////////////////////////////////////////////////

UIAtomLight::UIAtomLight()
{
}

UIAtomLight::~UIAtomLight()
{
}

void UIAtomLight::setShape(UIAtomLight::Shape s)
{
    shape = s;
}

void UIAtomLight::setOnColor(juce::Colour c)
{
    onColor = c;
}

void UIAtomLight::setOffColor(juce::Colour c)
{
    offColor = c;
}

void UIAtomLight::setOn(bool b)
{
    on = b;
}

void UIAtomLight::resized()
{
}

void UIAtomLight::paint(juce::Graphics& g)
{
    switch (shape) {
        case Circle: {
            // Ellipse wants float rectangles, getLocalBounds returns ints
            // seems like there should be an easier way to convert this
            juce::Rectangle<float> area (0.0f, 0.0f, (float)getWidth(), (float)getHeight());
            setBorderColor(g);
            g.drawEllipse(area, 2.0f);
            area = area.reduced(2.0f);
            setFillColor(g);
            g.fillEllipse(area);
        }
            break;
        case Square: {
            juce::Rectangle<int> area = getLocalBounds();
            setBorderColor(g);
            g.drawRect(area, 1);
            area = area.reduced(2);
            setFillColor(g);
            g.fillRect(area);
        }
            break;
        case Triangle: {
            setFillColor(g);
            g.fillRect(0, 0, getWidth(), getHeight());
        }
            break;
        case Star: {
            setFillColor(g);
            g.fillRect(0, 0, getWidth(), getHeight());
        }
            break;
    }
}

void UIAtomLight::setBorderColor(juce::Graphics& g)
{
    // todo: configurable border color and maybe no border
    g.setColour(juce::Colours::blue);
}

void UIAtomLight::setFillColor(juce::Graphics& g)
{
    if (on)
      g.setColour(onColor);
    else
      g.setColour(offColor);
}

//////////////////////////////////////////////////////////////////////
//
// Button
//
//////////////////////////////////////////////////////////////////////

UIAtomButton::UIAtomButton()
{
    onColor = juce::Colours::red;
    offColor = juce::Colours::black;
    backColor = juce::Colours::grey;
    overColor = juce::Colours::white;
}

UIAtomButton::~UIAtomButton()
{
}

void UIAtomButton::setListener(Listener* l)
{
    listener = l;
}

void UIAtomButton::setText(juce::String s)
{
    text = s;
}

void UIAtomButton::setOnColor(juce::Colour c)
{
    onColor = c;
}

void UIAtomButton::setOffColor(juce::Colour c)
{
    offColor = c;
}

void UIAtomButton::setOn(bool b)
{
    on = b;
}

void UIAtomButton::resized()
{
}

/**
 * drawFittedText
 * arg after justification is maximumNumberOfLines
 * which can be used to break up the next into several lines
 * last arg is minimumHorizontalScale which specifies
 * "how much the text can be squashed horizontally"
 * set this to 1.0f to prevent horizontal squashing
 * what that does is unclear, leaving it zero didn't seem to adjust
 * the font height, 0.5 was smaller but still not enough for
 * "This is a really long name"
 * adding another line didn't do anything, maybe the area needs
 * to be a multiple of the font height?
 * What I was expecting is that it would scale the font height down
 * to make everything smaller, but it doesn't seem to do that
 * setting this to 1.0f had the effect I expected, the font got smaller
 * and it used multiple lines.  So if this isn't 1.0 it prefers
 * squashing over font manipulation and mulitple lines.
 * "This is a name longer than anyone should use" mostly displayed
 * it lots the first "T" and the "n" at the end of "than" on the first
 * line, and the second line was complete and centered.  But
 * smaller names are okay, and I've spent enough time trying to figure
 * out exactly how this works.
 *
 * Still getting some truncation on the left and right for text that
 * fits mostly on one line "This is a long" will lose the left half
 * of the initial T and half of the final g.  Don't know if this is
 * an artifact of drawFittedText, or if I have bounds messed up somewhere.
 */
void UIAtomButton::paint(juce::Graphics& g)
{
    if (over)
      g.setColour(overColor);
    else
      g.setColour(backColor);
    g.fillRect(getLocalBounds());
    
    if (on)
      g.setColour(onColor);
    else
      g.setColour(offColor);

    juce::Font font(JuceUtil::getFont(getHeight()));
    // hacking around the unpredictable truncation, if the name is beyond
    // a certain length, reduce the font height
    if (text.length() >= 10)
      font = JuceUtil::getFontf(getHeight() * 0.75f);
          
    // not sure about font sizes, we're going to use fit so I think
    // that will size down as necessary
    g.setFont(font);
    
    g.drawFittedText(text, 0, 0, getWidth(), getHeight(),
                     juce::Justification::centred,
                     1, // max lines
                     1.0f);
}

void UIAtomButton::mouseEnter(const juce::MouseEvent& event)
{
    // getParentComponent()->mouseEnter(event);
    (void)event;
    if (!over) {
        over = true;
        repaint();
    }
}

void UIAtomButton::mouseExit(const juce::MouseEvent& event)
{
    //getParentComponent()->mouseExit(event);
    (void)event;
    if (over) {
        over = false;
        repaint();
    }
}

void UIAtomButton::mouseDown(const juce::MouseEvent& event)
{
    // getParentComponent()->mouseDown(event);
    (void)event;
    if (!on) {
        on = true;
        if (listener != nullptr)
          listener->atomButtonPressed(this);
        repaint();
    }
}

void UIAtomButton::mouseDrag(const juce::MouseEvent& event)
{
    //getParentComponent()->mouseDrag(event);
    (void)event;
}

void UIAtomButton::mouseUp(const juce::MouseEvent& event)
{
    //getParentComponent()->mouseUp(e);
    (void)event;
    if (on) {
        on = false;
        repaint();
    }
}

//////////////////////////////////////////////////////////////////////
//
// Text
//
//////////////////////////////////////////////////////////////////////

UIAtomText::UIAtomText()
{
    onColor = juce::Colours::red;
    offColor = juce::Colours::yellow;
    backColor = juce::Colours::black;
}

UIAtomText::~UIAtomText()
{
}

void UIAtomText::setText(juce::String s)
{
    text = s;
    repaint();
}

void UIAtomText::setOnColor(juce::Colour c)
{
    onColor = c;
}

void UIAtomText::setOffColor(juce::Colour c)
{
    offColor = c;
}

void UIAtomText::setBackColor(juce::Colour c)
{
    backColor = c;
}

void UIAtomText::setOn(bool b)
{
    on = b;
}

void UIAtomText::setFlash(bool b)
{
    flash = b;
}

void UIAtomText::advance()
{
    ticks++;
    if (ticks > 10) {
        ticks = 0;
        if (flash) {
            if (on)
              on = false;
            else
              on = true;
            repaint();
        }
    }
}

void UIAtomText::resized()
{
}

void UIAtomText::paint(juce::Graphics& g)
{
    g.setColour(backColor);
    g.fillRect(getLocalBounds());
    
    if (on)
      g.setColour(onColor);
    else
      g.setColour(offColor);

    juce::Font font(JuceUtil::getFont(getHeight()));
    // hacking around the unpredictable truncation, if the name is beyond
    // a certain length, reduce the font height
    if (text.length() >= 10)
      font = JuceUtil::getFontf(getHeight() * 0.75f);
          
    // not sure about font sizes, we're going to use fit so I think
    // that will size down as necessary
    g.setFont(font);
    
    g.drawFittedText(text, 0, 0, getWidth(), getHeight(),
                     juce::Justification::centred,
                     1, // max lines
                     1.0f);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
