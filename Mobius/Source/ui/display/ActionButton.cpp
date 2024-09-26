/*
 * An extension of juce::Button to associate the visible
 * button with a Mobius Action.  These are arranged in a
 * configurable row by ActionButtons.
 * 
 * juce::ApplicationCommandManager feels a bit like our
 * ActionDispatcher, might be able to use that here for button/key
 * bindings but not sure how much that buys us.
 *
 * Smiple extending TextButton creates a nice usable button but attempting
 * to control the colors was futile.
 * 
 * TextButton has these ColourIds
 *   buttonColourId, buttonOnColourId, textColourOffId, textColourOnId
 *
 * Colors are fucking weird
 * Without setting anything background color is just slighly non-black and turns grey when pressed
 * textColourOffId sets the text color, but textColourOnId does nothing, probably
 * because this is not a toggle button.  It also turns a lighter grey when the mouse is over it.
 * So there are three colors in play here.
 *
 * buttonColourId only displays a lightened version of the color initially
 * When you click it it turns the desired color and stays there but clicking a second
 * button dims it and darkens the other.  It is behaving like a radio group.  If the window
 * loses focus, it dims, then goes back when it regains focus.
 *
 * buttonOnColourId does nothing if this is not a toggle button.
 * 
 * So there is a whole lot of automatic coloring UNLESS you set buttonOnColorId then
 * it starts behaving like a radio.
 *
 * To make this look like the old ones will probably have to override the draw methods
 * or use one of the more basic button classes.
 *
 * Calling setToggleState(true) even with buttonColourId reverts it back to the default
 * colors and change behavior.
 *
 * If you setToggleable(true) the background defaults to shades of grey, textColourOffId
 * is the default text color, and clicking does nothing.
 * setClickingToggleState does what you would expect except the text color turns white if you don't set it.
 * 
 * Can override the paintButton method which has this interesting comment:
 *   shouldDrawButtonAsHighlighted true if the button is either in the 'over' or 'down' state
 *   so there is built-in mouse tracking that changes colors when you are over it.
 *
 * Fuck it, override the paintButton method
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"

#include "../../model/UIConfig.h"
#include "../../model/UIAction.h"
#include "../../model/Symbol.h"
#include "../JuceUtil.h"

#include "Colors.h"
#include "ActionButtons.h"
#include "ActionButton.h"

ActionButton::ActionButton(ActionButtons* parent)
{
    actionButtons = parent;
    downTracker = false;
    downRight = false;
}

/**
 * Initialize the button to trigger an action defined by a DisplayButton.
 * The default rendering of button text is the symbol name followed by
 * parenthesized arguments.  If a user-defined name is speified that
 * is used instead.
 */
ActionButton::ActionButton(ActionButtons* parent, DisplayButton* src)
{
    setName("ActionButton");
    actionButtons = parent;
    
    // don't wait for mouse up
    setTriggeredOnMouseDown(true);

    juce::String symbolName = src->action;
    
    if (src->name.length() > 0) {
        // name override to avoid the default rendering
        setButtonText(src->name);
    }
    else if (symbolName.length() > 0) {
        juce::String buttonText = symbolName;
        if (src->arguments.length() > 0) {
            // formatting here could be more complicated with
            // normalization and capitalization
            buttonText += "(";
            buttonText += src->arguments;
            buttonText += ")";
        }
        // todo: add scope too?
        setButtonText(buttonText);
    }

    color = src->color;

    if (symbolName.length() > 0) {
        action.symbol = actionButtons->getProvider()->getSymbols()->intern(symbolName);
        CopyString(src->arguments.toUTF8(), action.arguments, sizeof(action.arguments));

        // kludge: This is what Binderator does for MIDI/key bindings
        // if the binding has a simple numeric argument, promote that
        // to the action value
        //
        // The binding panels can only deal with a single argument string, but both
        // the new and old action models prefer an integer argument (or in the new model the value)
        // and expected the argument string to be parsed into that.  Argument strings used to be
        // more compliccated but at the moment they're limited to just a single numeric value.
        // Need to rethink at what level we do this.  If any functions need the full string then
        // we should just pass that and leave the parsing to the function implementation.  If none do
        // then we should package and share this conversion somewhere.
        //
        // Here, we're not dealing with a Binding, but DisplayButton has the same argument string
        if (strlen(action.arguments) > 0) {
            if (IsInteger(action.arguments))
              action.value = ToInt(action.arguments);
        }
    }
    else {
        Trace(1, "ActionButton: DisplayButton with no symbol name\n");
    }
    
    // ActionButtons will now assign the buttons
    // a unique sustainId so we can be sustainable
    // triggers
}

/**
 * Initialie a button to trigger a action for a Symbol
 * associated with a script or sample.
 *
 * todo: since these don't come from a DisplayButton, we don't
 * yet have a way to save color preferences, it would have to be something
 * in the ScriptProperties or SampleProperties
 */
ActionButton::ActionButton(ActionButtons* parent, Symbol* src)
{
    setName("ActionButton");
    actionButtons = parent;
    
    // don't wait for mouse up
    setTriggeredOnMouseDown(true);

    // DisplayButton buttons use a () convention to show that there are arguments
    // we'll dispense with that
    setButtonText(src->getName());

    action.symbol = src;

    dynamic = true;
}

ActionButton::~ActionButton()
{
}

void ActionButton::setColor(int argb)
{
    color = argb;
    repaint();
}

/**
 * Calculate a good width for this button given a height.
 * 
 * TextButton does not appear to have a Font, it defaults to something.
 * 
 * Button doesn't seem to have a Font, it will apparently be an
 * unknown percentage of the given height.  So calling getStringWidth
 * won't be entirely accurate without knowing that percentage.  If we
 * use the full height the Font will be slightly larger than what will
 * end up being used which is okay for getting a width.  It will be a little
 * wider than necessary.
 *
 * Alternately TextButton has changeWidthToFitText which
 * "Resizes the button's width to fit neatly around its current text,
 * and gives it the specified height".  Let's start there.
 *
 * Now that we overrid the paintButton method we have more control over this.
 */
int ActionButton::getPreferredWidth(int height)
{
    // we're control Font now so don't need this
    // changeWidthToFitText(height);
    juce::Font font (JuceUtil::getFontf(height * 0.75f));
    int minWidth = font.getStringWidth(getButtonText());
    // add some padding around the edges
    minWidth += 20;
    return minWidth;
}

/**
 * Draw buttons the old way.
 */
void ActionButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown)
{
    juce::Colour background = juce::Colour(MobiusBlue);
    if (color != 0)
      background = juce::Colour(color);
    
    if (shouldDrawButtonAsHighlighted) {
        if (shouldDrawButtonAsDown) {
            paintButton(g, background, juce::Colour(MobiusRed));
        }
        else {
            paintButton(g, background, juce::Colour(MobiusYellow));
        }
    }
    else {
        paintButton(g, background, juce::Colours::black);
    }
}    

/**
 * Paint a basic rounded button with text in predictable colors
 * Since we're overriding paint, use a font that is .75 of the button height
 * Unclear what cornerSize means, try things until it looks right
 */
void ActionButton::paintButton(juce::Graphics& g, juce::Colour background, juce::Colour textColour)
{
    g.setColour(background);
    // can't fucking use Rectangle<int>
    juce::Rectangle<int> bounds = getLocalBounds();
    g.fillRoundedRectangle((float)bounds.getX(), (float)bounds.getY(),
                           (float)bounds.getWidth(), (float)bounds.getHeight(), 8.0f);
    g.setColour(textColour);
    g.setFont(JuceUtil::getFontf(getHeight() * 0.75f));
    g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred);
}    

//////////////////////////////////////////////////////////////////////
//
// Action
//
////////////////////////////////////////////////////////////////////////

UIAction* ActionButton::getAction()
{
    return &action;
}

void ActionButton::setTriggerId(int id)
{
    action.sustainId = id;
}

/**
 * Flag to track the down state of the button so ActionButtons
 * can watch for an up transition when it receives
 * ButtonListener:: buttonStateChanged.
 */
void ActionButton::setDownTracker(bool b, bool rmb)
{
    downTracker = b;
    downRight = rmb;
}

// Button already has isDown) so don't conflict with that
bool ActionButton::isDownTracker()
{
    return downTracker;
}

bool ActionButton::isDownRight()
{
    return downRight;
}
