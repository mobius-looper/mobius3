/**
 * A basic container component with automatic layout options.
 *
 * Still hacking here while learning.
 * For this to work, the child components must have all set their desired
 * sizes before we do layout.  In practice this means that all children must
 * be JLabel or Field or one of my others that do auto-sizing.  I don't think
 * ordinary Juce components do that.
 *
 * Hmm, I thought we would calculate and maintain a fixed size but think of this
 * more like a min/max size and adapt to parent bounds that are larger.  We should be
 * able to float within it.  Is that always up to the parent to lay out or
 * should this provide some streatching options?
 * 
 * Control flow is unclear here.  Typically a parent will call
 * setSize or setBounds and then resized() is called indirectly.
 *
 * Various comments scrapped off the webz:
 *
 * There is no reason not to call resized(). If you do your layout of child components in resized, then its a common pattern to call resized after adding or removing a child component.
 *
 * There are no side-effects of calling that function, and the default implementation does nothing at all, so there’s really no reason you should not be able to call that if you need to re-arrange your child components or something.
 *
 * I'm thinking since it is probably not reliable to overload all of the
 * possible sizing methods and ignore them that we should instead focus on the
 * parents and have them NOT change size.  This requires coordination between parent
 * and child makes these components not generally useful in general Juce usage.
 * Which is fine for me, but I still wonder if it is possible to do this?
 *
 *    Random parent::resized
 *       tell auto sizing compenent to set bounds to something
 *
 *    AutoSizingComponent::setSize/resized
 *       ignore what the parent wants and set our size back to what we want
 *
 * I think this is probably not an issue because the parent expects the child
 * to obey and you shouldn't be using parents that want to change sizes, this
 * probably means FlexGrid which is fine since I don't use it.
 */

#include <JuceHeader.h>
#include "Panel.h"

Panel::Panel()
{
    // initialized orientation is Vertical
    init();
}

Panel::Panel(Orientation o)
{
    init();
    orientation = o;
}

void Panel::init()
{
    setName("Panel");
}

Panel::~Panel()
{
}

void Panel::setOrientation(Orientation o)
{
    orientation = o;
    // TODO: assume this is a constructor time thing
    // don't really need to support dynamic orientation changes
}
    
void Panel::addOwned(juce::Component* c)
{
    addAndMakeVisible(c);
    ownedChildren.add(c);
}

void Panel::addShared(juce::Component* c)
{
    addAndMakeVisible(c);
}

int Panel::getPreferredWidth()
{
    int width = 0;

    const juce::Array<Component*>& children = getChildren();
    for (int i = 0 ; i < children.size() ; i++) {
        Component* child = children[i];
        int cwidth = child->getWidth();
        if (orientation == Vertical) {
            if (cwidth > width)
              width = cwidth;
        }
        else {
            width += cwidth;
        }
    }

    return width;
}

int Panel::getPreferredHeight()
{
    int height = 0;
    
    const juce::Array<Component*>& children = getChildren();

    for (int i = 0 ; i < children.size() ; i++) {
        Component* child = children[i];
        int cheight = child->getHeight();
        if (orientation == Vertical) {
            height += cheight;
        }
        else {
            if (cheight > height)
              height = cheight;
        }
    }

    return height;
}

/**
 * Call this after the child hierarchy has been constructed
 * to calculate the desired minimum size.  
 */
void Panel::autoSize()
{
    // this will indirectly call resized() to do layout
    setSize(getPreferredWidth(), getPreferredHeight());
}

/**
 * Layout the child components within our current size.
 */
void Panel::resized()
{
    //auto area = getLocalBounds();
    const juce::Array<Component*>& children = getChildren();

    // TODO: allow some optional padding around the edges
    int left = 0;
    int top = 0;

    for (int i = 0 ; i < children.size() ; i++) {
        Component* child = children[i];
        child->setTopLeftPosition(left, top);
        if (orientation == Vertical) {
            top += child->getHeight();
        }
        else {
            left += child->getWidth();
        }
    }
}
