
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../script/ScriptRegistry.h"
#include "../JuceUtil.h"

#include "ScriptDetails.h"
#include "ScriptWindow.h"

ScriptWindow::ScriptWindow(Supervisor* s) : content(s),
    juce::DocumentWindow("Script Editor", juce::Colours::black, juce::DocumentWindow::allButtons)
{
    supervisor = s;
    
    // bounds of the entire display, reduced by a comfortable edge
    juce::Rectangle<int> displayArea = JuceUtil::getDisplayArea();
    Trace(2, "ScriptWindow: Display area %d %d %d %d",
          displayArea.getX(), displayArea.getY(), displayArea.getWidth(), displayArea.getHeight());

    // demo uses a RectanglePlacement to more easily orient things the top left
    // or bottom right corners, this seems useful but unnecessary if you always
    // want it top left, UNLESS it's possible for the display area to have an origin
    // other than 0,0, use it anyway as an example

    // the size you want the window to be
    juce::Rectangle<int> area (0, 20, 400, 800);

    // from the demo, orient it relative to the display area
    juce::RectanglePlacement placement (juce::RectanglePlacement::xLeft
                                        | juce::RectanglePlacement::yTop
                                        | juce::RectanglePlacement::doNotResize);

    // apply the desired size within the display area
    auto result = placement.appliedTo (area, displayArea);

    // play around with native vs. non-native window adornment
    // kinda liking the way Juce title bars look, not thrilled with
    // the corner drag widget, leave as native to match the main window
    bool native = true;
        
    // it appears that when you ask for a native title bar, it is displayed ABOVE
    // the origin of the window area, even the demo clipped that
    // 20 was a guess, it still looks closer to the top than the non-native title bar
    // might be a way to query what the native height actually is
    if (native)
      result.setY(result.getY() + 20);
        
    Trace(2, "ScriptWindow: Placement %d %d %d %d",
          result.getX(), result.getY(), result.getWidth(), result.getHeight());
        
    setBounds (result);

    // second arg is useBottomRightCornerResizer
    setResizable (true, !native);
    setUsingNativeTitleBar (native);
    //setVisible (true);

    // the demo seems to be wrong, it calls setContentOwned on a member
    // ColorSelector object, why?  one forum post also agrees it is wrong
    setContentNonOwned(&content, false);

    juce::Rectangle<int> contentArea = getLocalBounds();
    // skip over the title bar, must be a better way to get this
    // height of the window buttons are 26 but the bar has more height
    if (!native)
      contentArea.removeFromTop(40);
    content.setBounds(contentArea);

    JuceUtil::dumpComponent(this);
}

ScriptWindow::~ScriptWindow()
{
}

void ScriptWindow::closeButtonPressed()
{
    // this is what the demos do, but for me I think I want to keep it alive
    // holding state
    //delete this;
    setVisible(false);
}

// having this messes up the title bar
/*
void ScriptWindow::resized()
{
    //content.setBounds(getLocalBounds());
}
*/

void ScriptWindow::load(ScriptRegistry::File* file)
{
    content.load(file);
}

//////////////////////////////////////////////////////////////////////
//
// Content
//
//////////////////////////////////////////////////////////////////////

ScriptWindowContent::ScriptWindowContent(Supervisor* s) : editor(s)
{
    addAndMakeVisible(editor);
}

ScriptWindowContent::~ScriptWindowContent()
{
}

void ScriptWindowContent::load(ScriptRegistry::File* file)
{
    editor.load(file);
}

void ScriptWindowContent::resized()
{
    editor.setBounds(getLocalBounds());
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
