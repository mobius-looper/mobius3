/**
 * Common subclass for all popup panels.
 * 
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "JuceUtil.h"
#include "BasePanel.h"

BasePanel::BasePanel()
{
    addAndMakeVisible(resizer);
    resizer.setBorderThickness(juce::BorderSize<int>(BorderWidth));
    resizeConstrainer.setMinimumHeight(20);
    resizeConstrainer.setMinimumWidth(20);

    closeButtons.setListener(this);
    closeButtons.setCentered(true);
    closeButtons.add(&okButton);
    addAndMakeVisible(closeButtons);

    borderColor = juce::Colours::white;
}

BasePanel::~BasePanel()
{
}

void BasePanel::setContent(juce::Component* c)
{
    if (c != nullptr) {
        contentComponent = c;
        addAndMakeVisible(c);
    }
}

void BasePanel::resetButtons()
{
    closeButtons.clear();
}

void BasePanel::addButton(juce::Button* b)
{
    closeButtons.add(b);
}

/**
 * Hack subclasses can use if they don't want a title but also
 * want to al low drag.  Forwards mouse events from the children
 * back up to the base.
 */
void BasePanel::followContentMouse()
{
    if (contentComponent != nullptr)
      contentComponent->addMouseListener(this, true);
}

void BasePanel::setTitle(juce::String argTitle)
{
    title = argTitle;
    repaint();
}

void BasePanel::setBorderColor(juce::Colour c)
{
    borderColor = c;
}

void BasePanel::setAlert()
{
    borderColor = juce::Colours::yellow;
}

void BasePanel::show()
{
    // PanelFactor should only call this if we're not visible
    // I guess it doesn't hurt to check again
    if (!isVisible()) {
        showing();
        setVisible(true);
    }
}    

void BasePanel::close()
{
    if (isVisible()) {
        hiding();
        setVisible(false);
    }
}

void BasePanel::showing()
{
    //JuceUtil::dumpComponent(this);
}

void BasePanel::hiding()
{
}

void BasePanel::decacheForms()
{
}

void BasePanel::update()
{
}

void BasePanel::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    resizer.setBounds(area);

    // border
    area = area.reduced(BorderWidth);

    // title bar
    // only show this if there is a title
    // if there isn't have to assume the entire
    // content area is available for dragging
    if (title.length() > 0)
      area.removeFromTop(HeaderHeight);

    // footer
    juce::Rectangle<int> footerArea = area.removeFromBottom(FooterHeight);
    // a little air between the buttons and the border
    footerArea.removeFromBottom(4);
    closeButtons.setBounds(footerArea);

    // content gets what's left
    contentArea = area;
    if (contentComponent != nullptr)
      contentComponent->setBounds(area);
}

void BasePanel::setBackground(juce::Colour c)
{
    backgroundColor = c;
    backgroundColorSet = true;
}

void BasePanel::paint(juce::Graphics& g)
{
    juce::Rectangle<int> area = getLocalBounds();

    // drakgrey looks nice and highlights where the black input fields
    // are but it should be darker
    //g.fillAll (juce::Colours::black);
    if (backgroundColorSet)
      g.fillAll (backgroundColor);
    else
      g.fillAll (juce::Colours::darkgrey.darker(0.8f));

    g.setColour(borderColor);
    g.drawRect(area, BorderWidth);
    
    area = area.reduced(BorderWidth);

    if (title.length() > 0) {
        juce::Rectangle<int> header = area.removeFromTop(HeaderHeight);
        // g.setColour(juce::Colours::blue);
        // this is what ColorSelector shows, but strangely, if you take
        // the negative number from the uixonfig.xml and do an online dec-to-nex
        // the number is different and wrong
        //g.setColour(juce::Colour(0xFF1052E8));
        g.setColour(juce::Colour((juce::uint32)-15707416));
        g.fillRect(header);
        juce::Font font (JuceUtil::getFontf(HeaderHeight * 0.75f));
        // looks a little too thick without making the header taller
        //font.setBold(true);
        g.setFont(font);
        g.setColour(juce::Colours::white);
        g.drawText(title, header, juce::Justification::centred);
    }
}

void BasePanel::buttonClicked(juce::Button* b)
{
    if (b == &okButton) {
        close();
    }
    else {
        footerButton(b);
    }
}

void BasePanel::footerButton(juce::Button* b)
{
    (void)b;
}

//////////////////////////////////////////////////////////////////////
//
// Drag
//
//////////////////////////////////////////////////////////////////////

void BasePanel::mouseDown(const juce::MouseEvent& e)
{
    // limit drag to when the mouse is over the title bar if we have one
    // this is including the border, but the resizer seems to
    // have priority over the mouse event

    if (title.length() == 0 ||
        (e.getMouseDownY() < HeaderHeight + BorderWidth)) {
        
        dragger.startDraggingComponent(this, e);

        // the first arg is "minimumWhenOffTheTop"
        // set this to the full height and it won't allow dragging the
        // top out of bounds
        dragConstrainer.setMinimumOnscreenAmounts(getHeight(), 100, 100, 100);
        
        dragging = true;
    }
}

void BasePanel::mouseDrag(const juce::MouseEvent& e)
{
    dragger.dragComponent(this, e, &dragConstrainer);
    // haven't seen this in a long time, don't really need it
    if (!dragging)
      Trace(1, "BasePanel: mosueDrag didn't think it was dragging\n");
}

/**
 * Don't need any of this logic, it is left over from when I was
 * learning.
 */
void BasePanel::mouseUp(const juce::MouseEvent& e)
{
    if (dragging) {
        if (e.getDistanceFromDragStartX() != 0 ||
            e.getDistanceFromDragStartY() != 0) {

            // is this the same, probably not sensitive to which button
            if (!e.mouseWasDraggedSinceMouseDown()) {
                Trace(1, "BasePanel: Juce didn't think it was dragging\n");
            }
            
            //Trace(2, "BasePanel: New location %d %d\n", getX(), getY());
            
            //area->saveLocation(this);
            dragging = false;
        }
        else if (e.mouseWasDraggedSinceMouseDown()) {
            Trace(1, "BasePanel: Juce thought we were dragging but the position didn't change\n");
        }
    }
    else if (e.mouseWasDraggedSinceMouseDown()) {
        Trace(1, "BasePanel: Juce thought we were dragging\n");
    }

    dragging = false;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
