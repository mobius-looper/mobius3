/**
 * Slight extention to TabbedComponent that simplifies some things.
 * An eventual replacement for common/SimpleTabPanel.
 *
 * The TabbedComponent contains a TabbedButtonBar which you can manage
 * in various ways, but mostly this can be done just through TabbedComponent.
 * 
 * TabbedComponent things of interest:
 *
 * setOrientation
 *   top, bottom, left, right
 * 
 * setTabBarDepth
 *   "Specifies how many pixels wide or high the tab bar should be"
 *   If oriented on the top or bottom this is the height.
 *   Unclear what the default is
 *
 * setOutline
 *    thickness of an outline drawn around the content component.
 *
 * setIndent
 *    gap to leave around the content component
 *
 * TabbedButtonBar things of interest
 *
 *  addTab
 *    can add tabs directly to the bar without yet setting a content component
 *
 * Unclear where the tab bar font comes from.
 *
 * Don't see TabbedComponent.setContentComponent so it appears you have to set
 * them as you call addTab
 *
 * By default, the selected tab isn't obvious, they're just black boxes with
 * white text.  With the TabbedButtonBar you can set these colors:
 *
 *   tabOutlineColourId - colour to use to draw an outline around the tabs
 *   tabTextColourId - colour to use to draw tab names
 *   frontOutlineColourId - colour to use to draw an outline around the currently selected tab
 *   frontTextColourId - colour to use to draw the currently selected tab name
 *
 *
 * Tab background color for the selected tab is subtle and not documented.
 * The demo sets the background to ResizableWindow::backgroundColourId which
 * is a dark grey, almost black but not completely. This seems to be the same
 * default colour used for most things like text buttons, the TextEditor, etc.
 *
 * The selected tab appears with this background color, and the other tabs
 * are a lighter shade of gray.  So it seems to just do a color transform
 * on the one background color you can set.  If you want fundamentally different
 * colors you'll have to override currentTabChanged and set the color manually.
 * 
 * To know when tabs change "Attach a ChangeListener to the button bar".
 * TabbedComponent apparently does this and has the currentTabChanged() virtual
 * method which you can override to change the tab background.
 *
 * The virtual popupMenuClickOnTab() is called when you right click on a tab
 * and is intended to be used to show a popup menu.
 *
 * Setting frontTextColourId worked to highlight the text of the selected tab
 * but tabOutlineColourId appeared to do nothing and I did not see any references
 * in Juce source code other than the definition.
 *
 */

#include <JuceHeader.h>
#include "BasicTabs.h"

BasicTabs::BasicTabs() :
    juce::TabbedComponent{juce::TabbedButtonBar::Orientation::TabsAtTop}
{
    setName("BasicTabs");

    // unlcear what the default if it is too small the shading difference
    // between the selected and unselected tab becomes harder to see
    // setTabBarDepth(20);

    // play with colors
    juce::TabbedButtonBar& bar = getTabbedButtonBar();
    // this doesn't do anything
    bar.setColour(juce::TabbedButtonBar::frontOutlineColourId, juce::Colours::red);
    // this works
    //bar.setColour(juce::TabbedButtonBar::frontTextColourId, juce::Colours::red);
}

BasicTabs::~BasicTabs()
{
}

void BasicTabs::add(juce::String name, juce::Component* content)
{
    // tabBackgroundColour applies to the background of the tab button
    // and I think a default content component underneath the one you give it?
    // fourth arg is deleteComponentWhenNotNeeded
    // fifth is insertIndex=-1

    // from the demo
    // setting this to all black did not work well, there appeared to be no
    // difference in shading between the selected and unselected tabs
    auto colour = findColour (juce::ResizableWindow::backgroundColourId);

    addTab(name, colour, content, false);
}

void BasicTabs::show(int index)
{
    setCurrentTabIndex(index);
}

void BasicTabs::currentTabChanged(int newIndex, const juce::String& newName)
{
    // here we could set the background colors
    (void)newIndex;
    (void)newName;
}
