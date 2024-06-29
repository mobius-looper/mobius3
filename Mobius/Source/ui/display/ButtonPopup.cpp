/**
 * Popup window for editing single button properties, especially color.
 */

#include <JuceHeader.h>

#include "../../model/UIConfig.h"
#include "../../model/Symbol.h"
#include "../common/ColorSelector.h"

#include "Colors.h"
#include "ActionButton.h"
#include "ActionButtons.h"
#include "ButtonPopup.h"


ButtonPopup::ButtonPopup()
{
    addAndMakeVisible(&selector);

    commandButtons.setListener(this);
    commandButtons.setCentered(true);
    commandButtons.add(&oneButton, this);
    commandButtons.add(&sameButton, this);
    commandButtons.add(&allButton, this);
    commandButtons.add(&undoButton, this);
    commandButtons.add(&cancelButton, this);
    addAndMakeVisible(commandButtons);
}

ButtonPopup::~ButtonPopup()
{
}

void ButtonPopup::show(ActionButtons* buttons, ActionButton* button)
{
    allButtons = buttons;
    targetButton = button;
    
    int argb = button->getColor();
    if (argb == 0)
      argb = MobiusBlue;
    
    juce::OwnedArray<ActionButton>& current = allButtons->getButtons();
    for (auto b : current) {
        selector.addSwatch(b->getColor());
    }

    selector.setCurrentColour(juce::Colour(argb));
    
    juce::Point<int> point = buttons->getMouseXYRelative();

    // this will be MobiusDisplay which has most of the UI
    juce::Component* parent = buttons->getParentComponent();
    parent->addAndMakeVisible(this);

    // when it fits, open it to the immediate right/under the current mouse location
    // when we're near the right edge though, this has to be pushed to the left to
    // it doesn't clip outside the bounds of the parent
    int popupTop = point.getY();
    int popupWidth = 300;
    int popupLeft = point.getX();
    int popupRight = popupLeft + popupWidth;
    if (popupRight > parent->getWidth()) {
        popupLeft = parent->getWidth() - popupWidth;
        // since we're sliding it under the mouse, move it down a little
        popupTop += 8;
    }

    // we'll have the same clipping at the bottom, but that only happens if the
    // window was resized to be extremely short
    
    setBounds(popupLeft, popupTop, popupWidth, 200);
}

void ButtonPopup::close()
{
    allButtons->getParentComponent()->removeChildComponent(this);
    allButtons = nullptr;
    targetButton = nullptr;
}

void ButtonPopup::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    commandButtons.setBounds(area.removeFromBottom(20));
    selector.setBounds(area);
}

void ButtonPopup::buttonClicked(juce::Button* command)
{
    juce::Colour c = selector.getCurrentColour();
    int newColor = c.getARGB();
    int oldColor = targetButton->getColor();

    // save current colors for undo
    if (command != &cancelButton && command != &undoButton) {
        juce::Array<int> newUndo;
        juce::OwnedArray<ActionButton>& current = allButtons->getButtons();
        for (auto b : current) {
            newUndo.add(b->getColor());
        }
        undo.add(newUndo);
    }
    
    if (command == &oneButton) {
        change(targetButton, newColor);
    }
    else if (command == &sameButton) {
        juce::OwnedArray<ActionButton>& buttons = allButtons->getButtons();
        for (auto b : buttons) {
            if (b->getColor() == oldColor) {
                change(b, newColor);
            }
        }
    }
    else if (command == &allButton) {
        juce::OwnedArray<ActionButton>& buttons = allButtons->getButtons();
        for (auto b : buttons) {
            change(b, newColor);
        }
    }
    else if (command == &undoButton) {
        if (undo.size() > 0) {
            juce::Array<int> lastUndo = undo.removeAndReturn(undo.size() - 1);
            juce::OwnedArray<ActionButton>& buttons = allButtons->getButtons();
            for (int i = 0 ; i < lastUndo.size() ; i++) {
                int old = lastUndo[i];
                ActionButton* b = buttons[i];
                if (b != nullptr)
                  change(b, old);
            }
        }
    }

    close();
}

/**
 * Change the color of a button, and update the UIConfig to have that color.
 * 
 * The model wasn't designed well to go this direction,
 * to locate the DisplayButton assume the buttons UIAction symbol name
 * matches the action of the DisplayButton.
 */
void ButtonPopup::change(ActionButton* b, int color)
{
    UIConfig* config = Supervisor::Instance->getUIConfig();
    ButtonSet* buttonSet = config->getActiveButtonSet();

    // if this is MobiusBlue, collapse back down to zero
    if (color == MobiusBlue) color = 0;

    UIAction* action = b->getAction();
    if (action == nullptr) {
        Trace(1, "ActionButtons: Can't color a button without an action\n");
    }
    else if (action->symbol == nullptr) {
        Trace(1, "ActionButtons: Can't color a button with an unresolved symbol\n");
    }
    else {
        DisplayButton* db = buttonSet->getButton(action);
        if (db == nullptr) {
            Trace(1, "ActionButtons: Can't color unmatched button %s\n", action->symbol->getName());
        }
        else {
            b->setColor(color);
            // always save it or wait for shutdown?
            // setting dirty doesn't work, and is unreliable anyway
            // because something else can cause UIConfig to get reloaded
            // save it now
            db->color = color;
            config->dirty = true;
            Supervisor::Instance->updateUIConfig();
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
