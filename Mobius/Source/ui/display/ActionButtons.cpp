/*
 * Arranges a configurable list of ActionButtons in a row with auto
 * wrapping and sizing.
 *
 * Reads the list of buttons to display from UIConfig
 */

#include <JuceHeader.h>

#include "../../Supervisor.h"
#include "../../model/UIConfig.h"
#include "../../model/Binding.h"
#include "../../model/DynamicConfig.h"
#include "../../model/Symbol.h"
#include "../../model/ScriptProperties.h"
#include "../../model/SampleProperties.h"

#include "../JuceUtil.h"

#include "ActionButton.h"
#include "ButtonPopup.h"
#include "ActionButtons.h"
#include "MobiusDisplay.h"

const int ActionButtonsRowGap = 1;

ActionButtons::ActionButtons(MobiusDisplay* argDisplay)
{
    setName("ActionButtons");
    display = argDisplay;
}

ActionButtons::~ActionButtons()
{
}

/**
 * Rebuild the buttons from the UIConfig, and add any script/sample
 * symbols that ask for buttons.
n */
void ActionButtons::configure()
{
    UIConfig* config = Supervisor::Instance->getUIConfig();
    buildButtons(config);

    DynamicConfig* dynconfig = Supervisor::Instance->getDynamicConfig();
    dynamicConfigChanged(dynconfig);
}

void ActionButtons::addButton(ActionButton* b)
{
    b->addListener(this);
    addAndMakeVisible(b);
    buttons.add(b);
}

/**
 * Note that this deletes the button.
 * buttons.remove will call the destructor.
 */
void ActionButtons::removeButton(ActionButton* b)
{
    b->removeListener(this);
    removeChildComponent(b);
    // yuno remove(b) ?
    int index = buttons.indexOf(b);
    if (index >= 0)
      buttons.remove(index);
}

/**
 * Code is more complicated than it could be after the
 * configurationListener design addition.  Before we initialized
 * buttons in two phases.  First Bindings were read out of MobiusConfig
 * for buttons defined in the config UI.  Second when notification
 * was received of a DynamicConfig change after loading scripts or samples
 * and those objects were flagged for automatic buttons.  Since these happened
 * at different times, buildButtons and dynamicConfigChanged did complicated
 * merging that we don't need any more.  Merge these someday...
 *
 * We do now get button definitions from the UIConfig rather than the BindingSet
 * in MobiusConfig, but it is still done in two parts.
 *
 * Here we do just the UIConfig buttons and preserve the dynamic
 * buttons and dynamicConfigChanged will do the reverse.
 */
void ActionButtons::buildButtons(UIConfig* config)
{
    // find the things to keep
    juce::Array<ActionButton*> dynamicButtons;
    for (int i = 0 ; i < buttons.size() ; i++) {
        ActionButton* b = buttons[i];
        if (b->isDynamic()) {
            dynamicButtons.add(b);
        }
    }

    // remove everything from the parent component
    for (int i = 0 ; i < buttons.size() ; i++) {
      removeChildComponent(buttons[i]);
    }

    // remove the ones we want to keep from the owned button list
    for (int i = 0 ; i < dynamicButtons.size() ; i++) {
        ActionButton* b = dynamicButtons[i];
        buttons.removeAndReturn(buttons.indexOf(b));
    }

    // delete what's left
    buttons.clear();

    // add the UIConfig buttons
    ButtonSet* buttonSet = config->getActiveButtonSet();
    for (auto button : buttonSet->buttons) {
        ActionButton* b = new ActionButton(button);
        addButton(b);
    }
    
    // and restore the dynamic buttons
    for (int i = 0 ; i < dynamicButtons.size() ; i++) {
        addButton(dynamicButtons[i]);
    }

    assignTriggerIds();
}

/**
 * Rebuild the button list after the SymbolTable (formerly DynamicConfig)
 * changed. Here we preserve the MobiusConfig buttons, and rebuild
 * the dynamic buttons.
 *
 * If the button was manually configured then leave it in place.
 * Otherwise completely rebuild the dynamic button list at the end.
 * Order will be random which is enough for now.
 *
 * DynamicConfig is still passed but it will be empty now.  We derive
 * the script/sample buttons from the Symbols table.
 */
void ActionButtons::dynamicConfigChanged(DynamicConfig* config)
{
    (void)config;
    bool changes = false;
    
    // don't like iteration index assumptions so build
    // an intermediate list
    juce::StringArray thingsToKeep;
    juce::Array<ActionButton*> thingsToRemove;
    
    for (int i = 0 ; i < buttons.size() ; i++) {
        ActionButton* b = buttons[i];
        if (b->isDynamic()) {
            thingsToRemove.add(b);
        }
        else {
            thingsToKeep.add(b->getButtonText());
        }
    }
    
    // remove all the current dynamic buttons
    for (int i = 0 ; i < thingsToRemove.size() ; i++) {
        ActionButton* b = thingsToRemove[i];
        removeButton(b);
        // removeButton uses button.remove which deletes it
        changes = true;
    }

    // now add back the ones that aren't already there
    // this formerly used a list of DynamicActions in the DynamicConfig
    // now we use SymbolTable and look for BehaviorScript or BehaviorSample
    // symbols that have the auto-binding "button" flag set
    
    for (auto symbol : Symbols.getSymbols()) {
        if ((symbol->script && symbol->script->button) ||
            (symbol->sample && symbol->sample->button)) {
            // did it we have it manually configured?
            // todo: if we start hiding prefixes this will be more complicated
            if (!thingsToKeep.contains(symbol->getName())) {
                ActionButton* b = new ActionButton(symbol);
                addButton(b);
                changes = true;
            }
        }
    }

    assignTriggerIds();

    // okay, this is a weird one
    // it is not obvious to me how to do dynamic child component sizing with
    // the Juce way of thinking which is normally top down
    // we know we're contained in MobiusDisplay whose resized() method
    // will call down to our layout() so that should do the needful though
    // it's not good encapsulation
    if (changes) {
        display->resized();
    }
}

/**
 * Once we've built the ActionButton list, assign each one
 * a unique trigger id for their UIAction.  This is necessary
 * if you want them to behave as momentary triggers with
 * long-press or sustain behavior.
 *
 * There is a very rare race condition here where if you were holding
 * a button at the same time as the button list was rebuilt.
 * ActionButtons might change ids, and TriggerState down in the core
 * would then be tracking the wrong button.  For that to happen though you
 * would have to be holding down a button with the mouse and at the same time
 * do something that results in button rebuilding, like editing the button bindings
 * (unpossible), reloading the UIConfig (also unpossible), reloading scripts via
 * the dynamicConfigChanged callback and some of the scripts have !button declarations.
 * That least one is the only one even remotely possible, and it would take effort
 * to make that happen.  Scripts don't reload by themselves.
 * The "failure" mode should someone be bored enough to try that could be an up
 * transition that is ignored resulting in a long press that doesn't end.
 * Not catestrophic and not worth the effort to try and prevent.
 */
void ActionButtons::assignTriggerIds()
{
    // trigger type is TriggerUI and we're the only ones that can send actions
    // right now, if other components start doing that we'll need to coordinate
    // the assignment of ids so they don't overlap
    // for now just number them from 1
    for (int i = 0 ; i < buttons.size() ; i++) {
        ActionButton* b = buttons[i];
        b->setTriggerId(i+1);
    }
}

/**
 * Size oursleves so we can fit all the buttons.
 * Buttons are of variable width depending on their text since
 * some, especially with arguments, might be long.  Continue on one
 * line wrapping when we get to the end of the available space.
 * Expected to be called by the parent's resized() method when
 * it knows the available width.  Our resized() method will then
 * do nothing since the work has been done.
 *
 * Alternately could have a getPreferredHeight and a normal
 * resized() but we would have to do the same positioning calculations
 * in both so why bother.
 *
 * NB: There is a very subtle issue with layout() doing both layout
 * and sizing.  The buttons appear like expected but they are not
 * responsive.  It seems Button requires the parent to be of a predetermined
 * size BEFORE adding the children to initialize mouse sensitive areas
 * or something.  If you position and size the buttons, THEN set the parent
 * size it doesn't work.  Unfortunate because getPreferredHeight does all the
 * work, then we set parent bounds, then we do that work again to layout the
 * buttons.
 *
 * Or it might be something about breaking the usual rules of resized not
 * cascading to the children even though they are already in the desired positions.
 *
 * Well it started working for no obvious reason.
 * If you get here again, what seems to be always reliable is for parent::resized
 * to call getPreferredHeight, then have it call setBounds again even though
 * layout will have already done that, then in our resized() do the layout again.
 * That lead to unresponsive buttons, but after flailing around it started working
 * so I'm not sure what the magic was.
 */
void ActionButtons::layout(juce::Rectangle<int> bounds)
{
    int availableWidth = bounds.getWidth();
    int topOffset = 0;
    int leftOffset = 0;
    // todo: make this configurable ?
    int buttonHeight = 25;

    // before we start, bound the parent with all of the available size
    // so the button mouse listeners have something to work with
    // note that if this calls resized() it will be ignored
    // hmm, this didn't work
    //setBounds(bounds);

    int rowStart = 0;
    for (int i = 0 ; i < buttons.size() ; i++) {
        ActionButton* b = buttons[i];
        int minWidth = b->getPreferredWidth(buttonHeight);
        b->setSize(minWidth, buttonHeight);

        // leave a gap
        if (leftOffset > 0) leftOffset += 2;
        
        if (leftOffset + minWidth > availableWidth) {
            // center the current row and start a new one
            // I suppose we should handle the edge case where
            // the available width isn't enough for a single button
            // but just let it truncate
            centerRow(rowStart, i, leftOffset, availableWidth);
            leftOffset = 0;
            topOffset += buttonHeight + ActionButtonsRowGap;
            rowStart = i;
        }
        b->setTopLeftPosition(leftOffset, topOffset);
        leftOffset += minWidth;
    }

    // close off the last row
    if (leftOffset > 0) {
        centerRow(rowStart, buttons.size(), leftOffset, availableWidth);
        topOffset += buttonHeight + ActionButtonsRowGap;
    }

    // now adjust our height to only use what we needed
    setSize(availableWidth, topOffset);
}

/**
 * Hack trying to work around the unresponsive buttons problem.
 * Not used right now but keep around in case we have to resurect that.
 */
int ActionButtons::getPreferredHeight(juce::Rectangle<int> bounds)
{
    layout(bounds);
    return getHeight();
}

void ActionButtons::centerRow(int start, int end, int rowWidth, int availableWidth)
{
    int centerOffset = (availableWidth - rowWidth) / 2;
    for (int i = start ; i < end ; i++) {
        ActionButton* b = buttons[i];
        // so we have a getX but not a setX, makes perfect sense
        b->setTopLeftPosition(b->getX() + centerOffset, b->getY());
    }
}

/**
 * This is an unusual component in that the parent is expected to
 * call layout() rather than just setSize() in it's resized() method.
 * Since that does the work, when we eventually get to Juce calling
 * our resized() we don't have to do anything.
 *
 * See comments for layout() about a subtle mouse sensitivity problem
 * that seemed to have introduced.  For a time I had to call layout()
 * again here after setting our size, but then it started working.
 */
void ActionButtons::resized()
{
    // shouldn't get here, parent is supposed to call layout() instead
    //layout(getLocalBounds());
}

void ActionButtons::paint(juce::Graphics& g)
{
    (void)g;
    // draw a border for testing layout
    //g.setColour(juce::Colours::red);
    //g.drawRect(getLocalBounds(), 1);
}

/**
 * Rather than having each ActionButton propagate the UIAction we'll
 * forward all the clicks up here and do it.
 */
void ActionButtons::buttonClicked(juce::Button* src)
{
    // weird that Juce allows RMB to be a normal click and not pass this
    // to buttonClicked, have to check modifiers
    auto modifiers = juce::ModifierKeys::getCurrentModifiers();
    if (modifiers.isRightButtonDown()) {
        // show the popup editor
        popup.show(this, (ActionButton*)src);
    }
    else {
        // todo: figure out of dynamic_cast has any performance implications
        ActionButton* ab = (ActionButton*)src;
        UIAction* action = ab->getAction();

        // if we're configured to allow sustain, then make it look like we can
        action->sustain = enableSustain;
        action->sustainEnd = false;
    
        Supervisor::Instance->doAction(action);
    }
}

/**
 * Trying to figure out if we can do sustain triggers
 *
 * Well we can but ButtonState makes it harder than it could be
 * there is no "up" state.  When you press the button
 * a buttonDown will be received.  When the button goes up
 * you will be in either buttonNormal or buttonOver depending
 * on whether you haved the mouse off the button while
 * it was held.
 *
 * You can't just assume that Normal means up, because you
 * get that if you just hover over the button and then hover
 * away, it goes from Over to Normal.
 *
 * To reliably track down then up, you have to remember
 * when a Down notification was received, and check that
 * the next time the state is anything other than Down.
 *
 * new: To get RMB menu behavior, have to ignore down transition
 * on the right button, which we do in buttonClicked, but we also
 * need to handle this when the button goes up.  At that point
 * we'll be in ButtonState::buttonNormal but the mouse button isn't
 * actually down at this point so checking ModifierKeys doesn't work.
 * We have to remember which button was pressed when we got to
 * buttonDown.
 */
void ActionButtons::buttonStateChanged(juce::Button* b)
{
    ActionButton* ab = (ActionButton*)b;
    juce::Button::ButtonState state = b->getState();

    switch (state) {
        case juce::Button::ButtonState::buttonNormal: {
            //Trace(2, "Button normal\n");
            if (ab->isDownTracker()) {
                // this is relatively unusual, you pressed on
                // it then moved the mouse away before releasing
                buttonUp(ab);
            }
        }
            break;

        case juce::Button::ButtonState::buttonOver: {
            //Trace(2, "Button over\n");
            if (ab->isDownTracker()) {
                // this is the usual case, you were still
                // over the button when it was relased
                buttonUp(ab);
            }
        }
            break;
            
        case juce::Button::ButtonState::buttonDown: {
            //Trace(2, "Button down\n");
            if (ab->isDownTracker()) {
                // this could be a problem, we've already
                // sent a down UIAction to the engine and
                // now we're going to do it again without
                // releasing the last one
                // TriggerState may figure that out but I'm not sure
                // should we force a buttonUp?
                Trace(1, "ActionButtons: Dupicate down state detected\n");
            }
            // don't have to do anything here, the buttonClicked
            // callback will do the action
            auto modifiers = juce::ModifierKeys::getCurrentModifiers();
            bool rmb = modifiers.isRightButtonDown();
            ab->setDownTracker(true, rmb);
        }
            break;
    }
}

/**
 * Called by the state tracker when we think the button
 * was released.  Send an "up" action so this button
 * can behave as a sustainable trigger.
 *
 * Only send an up transition if the Symbol is bound to sustainable
 * target.  Need to start a library of Symbol utilities for
 * things like this, so we don't load Symbol up with a bunch of random stuff.
 *
 * If the symbol is not associated with a coreFunction, assume not sustainable.
 * Most coreFunctions won't be sustainable either, but we don't have the
 * Function class accessible here, and FunctionDefinition doesn't have anything
 * in it yet.  
 * 
 */
void ActionButtons::buttonUp(ActionButton* b)
{
    if (b->isDownRight()) {
        // up transition of the right mouse button,
        // no current behavior since a menu is usually being displayed
    }
    else if (enableSustain) {
        // ignore 
        //Trace(2, "ActionButtons: Sending up action\n");
        UIAction* action = b->getAction();
        Symbol* s = action->symbol;
        if (s != nullptr && s->coreFunction != nullptr) {
            action->sustainEnd = true;
            Supervisor::Instance->doAction(action);
        }
    }
    b->setDownTracker(false, false);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
