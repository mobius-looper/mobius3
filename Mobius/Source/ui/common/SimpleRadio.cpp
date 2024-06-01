/*
 * Provides a basic labeled radio component.
 * 
 * Hey! See this in the demo
 * juce::Logger::outputDebugString (name + " Button changed to " + stateString);
 * Explore using that instead of OutputDebutStream
 *
 * Discussion of onClick
 * https://forum.juce.com/t/button-onclick/25795
 *
 * Demo does this:
 *   maleButton  .onClick = [this] { updateToggleState (&maleButton,   "Male");   };
 *
 * When I try to do this:
 *   b->onClick = [this] { updateToggleState (b); };
 * Compiler says:
 *   C3493 'b' cannot be implicitly captured because no default capture mode has been specified
 *
 * Figure this out someday, for now use Button::Listener
 * 
 */

#include <JuceHeader.h>

#include "Panel.h"
#include "SimpleRadio.h"

SimpleRadio::SimpleRadio()
{
    setName("SimpleRadio");
}

SimpleRadio::~SimpleRadio()
{
}

void SimpleRadio::setSelection(int index)
{
    if (buttons.size() == 0) {
        // haven't rendered yet
        initialSelection = index;
    }
    else {
        juce::ToggleButton* b = buttons[index];
        if (b != nullptr)
          b->setToggleState(true, juce::dontSendNotification);
    }
}

int SimpleRadio::getSelection()
{
    int selection = -1;
    for (int i = 0 ; i < buttons.size() ; i++) {
        juce::ToggleButton* b = buttons[i];
        if (b->getToggleState()) {
            selection = i;
            break;
        }
    }
    return selection;
}

void SimpleRadio::render()
{
    int totalWidth = 0;
    int maxHeight = 0;
    
    if (buttonLabels.size() > 0) {
        label.setText(labelText);
        addAndMakeVisible(label);
        
        totalWidth += label.getWidth();
        maxHeight = label.getHeight();
        
        for (int i = 0 ; i < buttonLabels.size() ; i++) {

            juce::ToggleButton* b = new juce::ToggleButton(buttonLabels[i]);
            addAndMakeVisible(b);
            buttons.add(b);

            // unable to make b->onClick work, see comments at the top
            b->addListener(this);
            
            // what is the scope of the radio group id, just within this component
            // or global to the whole application?
            // "To find other buttons with the same ID, this button will search through its sibling components for ToggleButtons, so all the buttons for a particular group must be placed inside the same parent component.
            // so it seems to be local which is good
            b->setRadioGroupId(1);

            // as usual we have sizing shit
            // have label text as well as the checkbox
            int guessWidth = 50;
            int guessHeight = 20;
            
            b->setSize(guessWidth, guessHeight);
            
            totalWidth += b->getWidth();
            if (b->getHeight() > maxHeight)
              maxHeight = b->getHeight();

            if (initialSelection == i)
              b->setToggleState(true, juce::dontSendNotification);
        }
    }

    setSize(totalWidth, maxHeight);
}            

/**
 * We are not really interested in buttonClicked, the thing
 * that created this wrapper component is.  Yet another level
 * of listener, really need to explore lambdas someday.
 *
 * Weird...when dealing with a radio group we get buttonClicked
 * twice, apparently once when turning off the current button
 * and again when turning another one on.  In the first state
 * none of the buttons will have toggle state true, don't call
 * the listener in that case.
 */
void SimpleRadio::buttonClicked(juce::Button* b)
{
    (void)b;
    // could be smarter about tracking selections without iterating
    // over the button list
    int selection = getSelection();
    // ignore notifications of turning a button off
    if (selection >= 0) {
        if (listener != nullptr)
          listener->radioSelected(this, getSelection());
    }
}

/**
 * By default button lables are painted on the right.
 * No obvious way to change that to the left.  Chatter suggests
 * overriding the paint method but it's not that hard to manage
 * an array of labels oursleves.  Someday...
 */
void SimpleRadio::resized()
{
    label.setTopLeftPosition(0, 0);
    int buttonOffset = label.getWidth();
    for (int i = 0 ; i < buttons.size() ; i++) {
        Component* c = buttons[i];
        // ugh, arguments are x,y not top,left
        c->setTopLeftPosition(buttonOffset, 0);
        buttonOffset += c->getWidth();
    }
}

