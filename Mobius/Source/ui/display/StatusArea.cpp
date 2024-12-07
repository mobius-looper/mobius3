/**
 * Manages a configurable set of display elements that show various
 * parts of the Mobius engine runtime state.
 *
 * The elements may be selectively enabled with their locations specified
 * by the user using mouse dragging.
 *
 * Elements are currently expected to size themselves using the getPreferred
 * methods though it might be interesting to have them grow or shrink depending on
 * the size of the containing area and the other elements being displayed.
 * Would be really cool to allow the user to specify sizes, so you might want a larger
 * counter, but a really small layer list, etc.
 *
 * Configuration of the elements is stored in UIConfig under the StatusArea/StatusElement
 * tags.  This corresponds to the old Locations/Location model.
 *
 * Unlike ActionButtons, since we have a fixed set of possible child
 * components we can keep them as member objects and don't have to
 * maintain an OwnedArray.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/UIConfig.h"
#include "../JuceUtil.h"

#include "MobiusDisplay.h"
#include "Colors.h"
#include "UIElement.h"
#include "UIElementStatusAdapter.h"

#include "StatusArea.h"

StatusArea::StatusArea(MobiusDisplay* parent) : display(parent)
{
    setName("StatusArea");

    addElement(&mode);
    addElement(&beaters);
    addElement(&meter);
    addElement(&counter);
    addElement(&floater);
    addElement(&parameters);
    addElement(&audioMeter);
    addElement(&layers);
    addElement(&alerts);
    addElement(&minorModes);
    addElement(&tempo);
    addElement(&loopWindow);
}

void StatusArea::addElement(StatusElement* el)
{
    // will be made visible later in configure()
    addChildComponent(el);
    elements.add(el);
}

StatusArea::~StatusArea()
{
}

Provider* StatusArea::getProvider()
{
    return display->getProvider();
}

class MobiusView* StatusArea::getMobiusView()
{
    return display->getMobiusView();
}

/**
 * We'll only receive these if the mouse is not over a child component.
 * If this is a right mouse click, open the main popup menu.
 */
void StatusArea::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
      display->getProvider()->showMainPopupMenu();
}

void StatusArea::update(class MobiusView* view)
{
    for (int i = 0 ; i < elements.size() ; i++) {
        StatusElement* el = elements[i];
        if (el->isVisible())
          el->update(view);
    }
}

void StatusArea::resized()
{
    // The elements will already have been positioned and sized by configure()
    // These don't respond to container size.
    // if any components have juce::Buttons we might have that weird mouse
    // sensitivity problem if the parent wasn't sized before sizing the children?

    // JuceUtil::dumpComponent(this);
}

void StatusArea::paint(juce::Graphics& g)
{
    (void)g;
}

/**
 * Callback from the elements to update the saved position after dragging.
 * Need to remember the new location in the UIConfig or else it will be lost
 * if anything else changes the config and canuses configure()
 * to be called.
 */
void StatusArea::saveLocation(StatusElement* element)
{
    UIConfig* config = display->getProvider()->getUIConfig();
    DisplayLayout* layout = config->getActiveLayout();

    if (captureConfiguration(layout, element))
      config->dirty = true;
}

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

/**
 * Child components for all possible elements were added during construction
 * here we make them visible based on the active layout and set their location
 * and size.
 */
void StatusArea::configure()
{
    UIConfig* config = display->getProvider()->getUIConfig();
    DisplayLayout* layout = config->getActiveLayout();

    // the layout must have an element definition for all possible elements
    for (auto element : layout->mainElements) {
        // we can only have StatusElements so safe to cast
        // if not, will ahve to use dynamic_cast
        StatusElement* el = (StatusElement*)findChildWithID(element->name);
        if (el == nullptr)
          el = createExtendedElement(element);

        if (el != nullptr) {
            el->configure();
            el->setTopLeftPosition(element->x, element->y);
            
            // saved sizes are messed up for ParametersElement and FloatingStripElement
            // make the subclasses ask for resize
            int width = el->getPreferredWidth();
            int height = el->getPreferredHeight();
            if (el->allowsResize()) {
                width = (element->width > 0) ? element->width : el->getPreferredWidth();
                height = (element->height > 0) ? element->height : el->getPreferredHeight();
            }
            
            el->setSize(width, height);
            el->setVisible(!element->disabled);
        }
    }

    // development hack
    // if there are any new elements that weren't previously known force them
    // to display so they can be repositined and saved
    for (int i = 0 ; i < elements.size() ; i++) {
        addMissing(elements[i]);
    }

    // option to force border/label drawing
    if (showBorders != config->showBorders) {
        showBorders = config->showBorders;
        repaint();
    }
}

/**
 * Here when there is an element reference in the layout that didn't have a matching
 * component in the child list.  This only happens for extended components
 * since the intrinsic components are always added as (possibly disabled)
 * children in the constructor.
 */
StatusElement* StatusArea::createExtendedElement(DisplayElement* ref)
{
    StatusElement* element = nullptr;
    UIConfig* config = display->getProvider()->getUIConfig();
    UIElementDefinition* def = config->findDefinition(ref->name);
    if (def == nullptr) {
        Trace(1, "StatusArea: Unknwon UIElement definition name %s",
              ref->name.toUTF8());
    }
    else {
        // a better factory for these somewhere?
        UIElement* uie = UIElement::createElement(display->getProvider(), def);
        if (uie != nullptr) {
            // temporary: wrap it in something that makes it look
            // like a StatusElement
            element = new UIElementStatusAdapter(this, uie);
            // once this is added as a child, it stays there and is enabled or
            // disabled, this ID is how configure() finds it
            element->setComponentID(ref->name);
            // this one is for Juce
            addChildComponent(element);
            // this one is for us
            elements.add(element);
            // and this one makes it go away
            extendedElements.add(element);
        }
    }
    return element;
}

/**
 * Hack to disable the usual display of the elements and instead display them
 * bordered with a label showing what they do.
 */
void StatusArea::setIdentify(bool b)
{
    if (b != identify) {
        identify = b;
        repaint();
    }
}

/**
 * If a status element is defined but was not in the layout
 * add it with default characteristics so it can be seen and dragged into place.
 * Temporary aid for development so we can add new elements without having
 * to remember to update the UIConfig
 */
void StatusArea::addMissing(StatusElement* el)
{
    if (el->getWidth() == 0) {
        Trace(2, "Boostrapping location for StatusElement %s\n",
              el->getComponentID().toUTF8());
        // didn't size it in configure
        // put them at top/left I guess, could center them but won't
        // know that till later
        el->setTopLeftPosition(0, 0);
        el->setSize(el->getPreferredWidth(), el->getPreferredHeight());
        el->setVisible(true);
    }
}

/**
 * Save configuration before exiting or when switching layouts.
 */
void StatusArea::captureConfiguration(UIConfig* config)
{
    DisplayLayout* layout = config->getActiveLayout();
    
    for (int i = 0 ; i < elements.size() ; i++) {
        StatusElement* el = elements[i];

        if (captureConfiguration(layout, el))
          config->dirty = true;
    }

    // going to be lazy and skip smart change detection, just write it every time
    config->dirty = true;
}

/**
 * Capture the location, size, and any other dynamic configuration
 * for one display element.  This is used by both captureConfiguration
 * on shutdown and saveLocation after an individual drag/resize.
 */
bool StatusArea::captureConfiguration(DisplayLayout* layout, StatusElement* el)
{
    bool changed = false;

    // the ComponentID is used as the persistent identifier and
    // must be set when it was added
    juce::String name = el->getComponentID();
    if (name.length() == 0) {
        // bad dog, bad
        Trace(1, "StatusElement with no ID %s, both angry and disappointed\n",
              el->getName().toUTF8());
    }
    else {
        DisplayElement* del = layout->getElement(name);
        if (del == nullptr) {
            // must have bootstrapped one that wasn't there before
            del = new DisplayElement();
            del->name = name;
            layout->mainElements.add(del);
        }

        // is smart change detection really that important?
        // easier just to write the UIConfig on exit every time
        // if we start adding more configurable things

        bool isDisabled = !el->isVisible();
        if (del->disabled != isDisabled) {
            del->disabled = isDisabled;
            changed = true;
        }

        if (del->x != el->getX()) {
            del->x = el->getX();
            changed = true;
        }

        if (del->y != el->getY()) {
            del->y = el->getY();
            changed = true;
        }

        // can't change sizes yet but go through the motion
        // only need to save if it differs from the default
        if (el->getWidth() != el->getPreferredWidth()) {
            if (del->width != el->getWidth()) {
                del->width = el->getWidth();
                changed = true;
            }
        }
        
        if (el->getHeight() != el->getPreferredHeight()) {
            if (del->height != el->getHeight()) {
                del->height = el->getHeight();
                changed = true;
            }
        }
    }

    return changed;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
