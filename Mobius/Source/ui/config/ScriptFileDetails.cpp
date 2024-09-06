/**
 * This is the first example of a little popup information window that is
 * not a full blown BasePanel.  Will probably want more of these so factor
 * out a superclass.
 */

#include <JuceHeader.h>

#include "../JuceUtil.h"
#include "../../script/ScriptRegistry.h"
#include "../../script/MslDetails.h"
#include "../../script/MslError.h"

#include "../script/ScriptDetails.h"

#include "ScriptFileDetails.h"

#define BorderWidth 2
#define FooterHeight 24
#define FooterPad 4

ScriptFileDetails::ScriptFileDetails()
{
    closeButtons.setListener(this);
    closeButtons.setCentered(true);
    closeButtons.add(&okButton);
    addAndMakeVisible(closeButtons);

    details.addMouseListener(this, true);
    addAndMakeVisible(details);

    // be smarter about sizing
    setBounds(0, 0, 500, 200);
}

ScriptFileDetails::~ScriptFileDetails()
{
}

void ScriptFileDetails::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    // border
    area = area.reduced(BorderWidth);
    
    // footer
    juce::Rectangle<int> footerArea = area.removeFromBottom(FooterHeight);
    // a little air between the buttons and the border
    footerArea.removeFromBottom(FooterPad);
    closeButtons.setBounds(footerArea);

    details.setBounds(area);
}

void ScriptFileDetails::paint(juce::Graphics& g)
{
    juce::Rectangle<int> area = getLocalBounds();

    g.fillAll (juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.drawRect(area, BorderWidth);
}

void ScriptFileDetails::buttonClicked(juce::Button* b)
{
    (void)b;
    setVisible(false);
}

void ScriptFileDetails::show(ScriptRegistry::File* file)
{
    details.load(file);
    
    if (!isVisible()) {
        if (!shownOnce) 
          JuceUtil::centerInParent(this);
        shownOnce = true;
        setVisible(true);
    }
    else {
        repaint();
    }
}

//
// Drag/Resize
//

void ScriptFileDetails::mouseDown(const juce::MouseEvent& event)
{
    dragger.startDraggingComponent(this, event);

    // the first arg is "minimumWhenOffTheTop"
    // set this to the full height and it won't allow dragging the
    // top out of bounds
    dragConstrainer.setMinimumOnscreenAmounts(getHeight(), 100, 100, 100);
        
    dragging = true;
}

void ScriptFileDetails::mouseDrag(const juce::MouseEvent& event)
{
    dragger.dragComponent(this, event, &dragConstrainer);
}

void ScriptFileDetails::mouseUp(const juce::MouseEvent& e)
{
    (void)e;
    dragging = false;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
