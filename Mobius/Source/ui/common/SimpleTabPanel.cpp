/*
 * Simple tab panel manager.
 *
 * Things to think about in TabbedComponent
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
 * Hmm, rather than calling setTabNames, followed by addTabComponent or something
 * can just have addTabComponent receive the tab name.  Saves having to have another
 * wrapper component to add to the tab to the bar and then add content later.
 *
 */

#include <JuceHeader.h>

#include "JLabel.h"
#include "Panel.h"

#include "SimpleTabPanel.h"

SimpleTabPanel::SimpleTabPanel()
{
    setName("SimpleTabPanel");
    addAndMakeVisible(tabs);
}

SimpleTabPanel::~SimpleTabPanel()
{
}

/**
 * tabBackgroundColor is the color for both the background of the tab button
 * and the default content component that holds the component we give it.
 */
void SimpleTabPanel::addTab(juce::String name, Component* content)
{
    // second arg is tabBackgroundColor
    // final arg is deleteComponentWhenNotNeeded
    // I think this means that when the tab is removed or when the
    // component is destructed
    // we'll assume they are managed higher up
    tabs.addTab(name, juce::Colours::darkgrey, content, false);
}

/**
 * Testing hack.
 */
void SimpleTabPanel::addTab(juce::String name)
{
    int ntabs = tabs.getNumTabs();
    
    Panel* panel = new Panel();
    JLabel* label = new JLabel(juce::String("Tab ") + juce::String(ntabs+1));
    panel->addOwned(label);
    panel->autoSize();
    
    tabs.addTab(name, juce::Colours::blue, panel, true);
}

void SimpleTabPanel::resized()
{
    tabs.setBounds(getLocalBounds());
}
 
void SimpleTabPanel::setBackgroundColor(juce::Colour color)
{
    for (int i = 0 ; i < tabs.getNumTabs() ; i++) {
        tabs.setTabBackgroundColour(i, color);
    }
}

void SimpleTabPanel::showTab(int index)
{
    // optional second arg is sendChangeMessage
    // set to -1 to deselect all, what does that show?
    tabs.setCurrentTabIndex(index, false);
}
