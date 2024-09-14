
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../JuceUtil.h"
#include "Colors.h"
#include "StatusArea.h"
#include "StatusElement.h"

/**
 * Base class constructor called only by the subclasses.
 * The name is set as the Component ID so we can search for it
 * with findChildWithID.  That will also be the name used
 * in the UILocaion model.
 *
 * For diagnostic trace we have so far used the Component name to
 * label them so duplicate it there.  The current expectation is that
 * Component name will be the class name which doesn't have to be the
 * same as the UILocation mame if you wanted to simplify them.
 * But keeping them in sync is fine for now.  Could also change
 * JuceUtil to use the componentID and use that consistently everywhere.
 */
StatusElement::StatusElement(StatusArea* parent, const char* name)
{
    setComponentID(name);
    setName(name);
    statusArea = parent;

    addAndMakeVisible(resizer);
    resizer.setBorderThickness(juce::BorderSize<int>(4));
}

StatusElement::~StatusElement()
{
}

/**
 * Overloaded by ParametersElement and FloatingStripElement.
 */
void StatusElement::configure()
{
}

void StatusElement::update(class MobiusView* view)
{
    (void)view;
}

MobiusView* StatusElement::getMobiusView()
{
    return statusArea->getSupervisor()->getMobiusView();
}

// these should probably be pure virtual
// any useful thing to do in a default implementation?

int StatusElement::getPreferredWidth()
{
    return 100;
}

int StatusElement::getPreferredHeight()
{
    return 20;
}

void StatusElement::resized()
{
    if (resizes)
      resizer.setBounds(getLocalBounds());
}

/**
 * todo: might want some default painting for borders, labels,
 * and size dragging.  Either the subclass must call back up to this
 * or we have a different paintElement function.
 */
void StatusElement::paint(juce::Graphics& g)
{
    if (mouseEntered || statusArea->isShowBorders() || statusArea->isIdentify()) {
        if (dragging)
          g.setColour(juce::Colour(MobiusPink));
        else if (mouseEntered)
          g.setColour(juce::Colours::white);
        else
          g.setColour(juce::Colour(MobiusBlue));
        g.drawRect(getLocalBounds(), 1);
    }

    if (statusArea->isIdentify() || (mouseEntered && mouseEnterIdentify)) {
        // by convention, we set the Component id to a useful name
        // with "Element" suffixed
        juce::String id = getComponentID();
        id = id.upToFirstOccurrenceOf("Element", false, false);
        juce::Font font (JuceUtil::getFont(12));
        g.setFont(font);
        g.drawText(id, 0, 0, getWidth(), getHeight(), juce::Justification::centred);
    }
}

/**
 * Subclasses must call this to decide whether to paint themselves.
 */
bool StatusElement::isIdentify()
{
    return statusArea->isIdentify();
}

//////////////////////////////////////////////////////////////////////
//
// Mouse Tracking
//
//////////////////////////////////////////////////////////////////////

void StatusElement::mouseEnter(const juce::MouseEvent& e)
{
    (void)e;
    mouseEntered = true;
    repaint();
}

void StatusElement::mouseExit(const juce::MouseEvent& e)
{
    (void)e;
    mouseEntered = false;
    repaint();
}

void StatusElement::mouseDown(const juce::MouseEvent& e)
{
    dragger.startDraggingComponent(this, e);
    dragging = true;
}

void StatusElement::mouseDrag(const juce::MouseEvent& e)
{
    dragger.dragComponent(this, e, nullptr);
}

void StatusElement::mouseUp(const juce::MouseEvent& e)
{
    if (dragging) {
        if (e.getDistanceFromDragStartX() != 0 ||
            e.getDistanceFromDragStartY() != 0) {

            // is this the same, probably not sensitive to which button
            if (!e.mouseWasDraggedSinceMouseDown()) {
                Trace(1, "StatusElement: Juce didn't think it was dragging\n");
            }
            // Trace(2, "StatusElement: New location %d %d\n", getX(), getY());
            statusArea->saveLocation(this);
            dragging = false;
        }
        else if (e.mouseWasDraggedSinceMouseDown()) {
            Trace(1, "StatusElement: Juce thought we were dragging but the position didn't change\n");
        }
    }
    else if (e.mouseWasDraggedSinceMouseDown()) {
        Trace(1, "StatusElement: Juce thought we were dragging\n");
    }

    dragging = false;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
