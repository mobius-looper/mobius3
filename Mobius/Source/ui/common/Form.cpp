/**
 * Component model for Mobius configuration forms.
 * 
 * A form consists of a list of FormPanels.  If there is more than
 * one panel a tabbed component is added to select the visible panel.
 *
 * Panels almost always contain a FieldGrid though they can have other things.
 *
 * In theory panels can contain more than one grid that we might like to
 * put a labeled group border around.  If we end up there will need
 * more complicate Field adder methods.
 */

#include <string>
#include <sstream>

#include <JuceHeader.h>

#include "FormPanel.h"
#include "Form.h"

Form::Form() :
    tabs {juce::TabbedButtonBar::Orientation::TabsAtTop}
{
    setName("Form");

    // adjust tab bar colors
    juce::TabbedButtonBar& bar = tabs.getTabbedButtonBar();
    bar.setColour(juce::TabbedButtonBar::ColourIds::tabTextColourId, juce::Colours::grey);
    bar.setColour(juce::TabbedButtonBar::ColourIds::frontTextColourId, juce::Colours::white);
            
    // add space between the bar and the grid
    // ugh, this leaves a border around it with the background color
    // may be better to twiddle with the content component?
    // doesn't really help because the first component is still adjacent to the
    // indent color 
    //tabs.setIndent(4);
}

Form::~Form()
{
}

// new interface for adding tabs that aren't FormPanels
// really need to get tab handling out of Form
void Form::addTab(juce::String name, juce::Component* c)
{
    tabs.addTab(name, juce::Colours::black, c, false);
}    

void Form::add(FormPanel* panel)
{
    panels.add(panel);
}

FormPanel* Form::getPanel(juce::String name)
{
    FormPanel* found = nullptr;
    for (int i = 0 ; i < panels.size() ; i++) {
        FormPanel* p = panels[i];
        if (name == p->getTabName()) {
            found = p;
            break;
        }
    }
    return found;
}

/**
 * Called during form rendering to add a field to a panel/grid
 * at the specified column.
 *
 * Making assumptions right now that each panel can contain only
 * one grid.
 *
 */
void Form::add(Field* f, const char* tab, int column)
{
    FormPanel* panel = nullptr;

    if (tab == nullptr) {
        // simple form, no tabs
        if (panels.size() > 0)
          panel = panels[0];
    }
    else {
        // find panel by ame
        for (int i = 0 ; i < panels.size() ; i++) {
            FormPanel* p = panels[i];
            if (p->getTabName() == tab) {
                panel = p;
                break;
            }
        }
    }

    if (panel == nullptr) {
        // give it something to catch errors
        if (tab == nullptr) tab = "???";
        panel = new FormPanel(juce::String(tab));
        panels.add(panel);
    }
    
    // once we allow panels to have more than
    // one grid will need to name them
    FieldGrid* grid = panel->getGrid(0);
    if (grid == nullptr) {
        grid = new FieldGrid();
        panel->addGrid(grid);
    }

    grid->add(f, column);


    // pass along the HelpArea if we were given one,
    // this has to be done before adding Fields
    // todo: work out a less invasive way of decorating
    // help providers
    f->setHelpArea(helpArea);
}

/**
 * Traverse the hierarchy to find all contained Fields.
 */
void Form::gatherFields(juce::Array<Field*>& fields)
{
    for (int i = 0 ; i < panels.size() ; i++){
        FormPanel* p = panels[i];
        if (p != nullptr)
          p->gatherFields(fields);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Rendering
//
//////////////////////////////////////////////////////////////////////

/**
 * If there is more than one grid, the grids are rendered
 * inside a TabbedComponent which will manage visibility.
 * Otherwise the single grid is added directly as a child
 * component of the form so we don't see a tab button bar.
 *
 * TabbedComponent rendering notes
 *
 * By default tab components add an indent around the content
 * component.  You can set this with setIndent but weirdly you
 * there is no getter to see what it is.  You can set it in render
 * and then later make that assumption in getMinimumSize for
 * auto-sizing.
 *
 * Outline
 *
 * Outline is an optional line that will be drawn around the content but
 * not the edge touching the tab bar.  For the usual top bar this means
 * no top line.  The content component will have an inset (x/y position)
 * of that amount.  For auto sizing this needs to be factored in to ensure
 * the total size is large enought.
 *
 * Indent
 *
 * "Specifies a gap to leave around the edge of the content component.
 * Each edge of the content component will be indented by the given number of pixels."
 *
 * Note that the background color given to the buttons on the tab bar
 * is also used to fill the content component.
 * 
 * If you do both the outline is outside the indent.
 */
void Form::render()
{
    // make sure the panels are rendered
    for (int i = 0 ; i < panels.size() ; i++) {
        FormPanel* panel = panels[i];
        panel->render();
    }

    // testing
    // left a gray outline
    // outlineWidth = 4;
    outlineWidth = 0;

    // not sure what this did
    indentWidth = 4;

    // only add a tab bar if we have more than one
    if (panels.size() > 1) {
        //tabs.setColour(juce::TabbedComponent::ColourIds::outlineColourId, juce::Colours::blue);
        tabs.setOutline(outlineWidth);
        tabs.setIndent(indentWidth);
        
        for (int i = 0 ; i < panels.size() ; i++) {
            FormPanel* panel = panels[i];
            // last flag is deleteComponentWhenNotNeeded
            // we manage deletion of the grids
            tabs.addTab(juce::String(panel->getTabName()), juce::Colours::black, panel, false);
        }
        // if for some reason you want something other than
        // the first tab selected you can call tis
        // tabs.setCurrentTabIndex(index, false);
        addChildComponent(tabs);
        tabs.setVisible(true);
    }
    else if (panels.size() > 0) {
        // only one, add it directly to the form
        FormPanel* first = panels[0];
        addChildComponent(first);
        // necessary?
        first->setVisible(true);
    }

    juce::Rectangle<int> size = getMinimumSize();
    setSize(size.getWidth(), size.getHeight());
}

juce::Rectangle<int> Form::getMinimumSize()
{
    int maxWidth = 0;
    int maxHeight = 0;
    
    for (int i = 0 ; i < panels.size() ; i++) {
        FormPanel* panel = panels[i];
        panel->autoSize();
        if (panel->getWidth() > maxWidth)
          maxWidth = panel->getWidth();
        if (panel->getHeight() > maxHeight)
          maxHeight = panel->getHeight();
    }

    // add in the tab button bar
    // hmm, should be looking at the font?
    // default height seems to be 30
    if (panels.size() > 1) {
        // juce::TabbedButtonBar& bar = tabs.getTabbedButtonBar();
        // bar.getHeight() is not set yet
        // tabs.getTabBarDepth() defaults to 30 and can be set
        int barHeight = tabs.getTabBarDepth();
        maxHeight += barHeight;

        // add the outline on the bottom and sides
        maxHeight += outlineWidth;
        maxWidth += (outlineWidth * 2);
        
        // add the indent on all four sides
        maxWidth += (indentWidth * 2);
        maxHeight += (indentWidth * 2);
    }

    return juce::Rectangle<int> {0, 0, maxWidth, maxHeight};
}
        
/**
 * Give all the tab grids the full size.
 * Need to factor out the tab buttons.
 */
void Form::resized()
{
    if (panels.size() > 1) {
        // we used a tab component
        // will this cascade to the child FormPanels?
        tabs.setSize(getWidth(), getHeight());
    }
    else {
        // is this necessary, wait shouldn't we be doing this for all of them?
        FormPanel* first = panels[0];
        first->setTopLeftPosition(0, 0);

        // If this was in a TabbedComponent it should have resized
        // the FormPanels to fill the available size.  Do the same here
        first->setSize(getWidth(), getHeight());
    }
}

void Form::paint(juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
