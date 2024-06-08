/**
 * A collection of FieldGrids that may be contained with a Form tab.
 *
 * Grids are expected to be heap allocated and ownership transfers
 * to the FormPanel.
 *
 * Normally there will only be one FieldGrid, if there is more than one
 * it is layed out vertically.
 * The panel name becomes the tab name if there is more than one
 * panel in a form.
 *
 * The Tracks panel for SetupPanel is unusual because it
 * has a track selection radio component at the top and some
 * buttons at the bottom to capture runtime state.
 *
 * We could try to implement these with stacked grids, but just
 * Allow for a list of header/footer components.  Center them above the grids.
 */

#include <JuceHeader.h>
#include "FormPanel.h"

FormPanel::FormPanel()
{
    setName("FormPanel");
}

FormPanel::FormPanel(juce::String argTabName)
{
    setName("FormPanel");
    tabName = argTabName;
}

void FormPanel::addHeader(juce::Component* c)
{
    addAndMakeVisible(c);
    header.add(c);
}

// kludge for SetupPanel to replace the track selector radio
// with a combo box, the whole Form design is old and a mess
// this is just a hack to get it working until I can redesign it
void FormPanel::replaceHeader(juce::Component* c)
{
    for (auto hcomp : header)
      removeChildComponent(hcomp);
    header.clear();

    addHeader(c);
}

void FormPanel::addFooter(juce::Component* c)
{
    addAndMakeVisible(c);
    footer.add(c);
}

void FormPanel::addGrid(FieldGrid* grid)
{
    grids.add(grid);
    addAndMakeVisible(grid);
}

FieldGrid* FormPanel::getGrid(int index)
{
    // juce::Array is supposed to deal with out of range indexes
    return grids[index];
}

void FormPanel::gatherFields(juce::Array<Field*>& fields)
{
    for (int i = 0 ; i < grids.size() ; i++) {
        FieldGrid* grid = grids[i];
        grid->gatherFields(fields);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Rendering and Layout
//
//////////////////////////////////////////////////////////////////////

/**
 * No special rendering, cascade ot the grids.
 * Calculate initial minimum size.
 */
void FormPanel::render()
{
    // make sure the grids are rendered
    for (int i = 0 ; i < grids.size() ; i++) {
        FieldGrid* grid = grids[i];
        grid->render();
    }
    
    juce::Rectangle<int> size = getMinimumSize();
    setSize(size.getWidth(), size.getHeight());
}

juce::Rectangle<int> FormPanel::getMinimumSize()
{
    int maxWidth = 0;
    int maxHeight = 0;
    
    for (int i = 0 ; i < header.size() ; i++) {
        Component* c = header[i];
        if (c->getWidth() > maxWidth)
          maxWidth = c->getWidth();
        if (c->getHeight() > maxHeight)
          maxHeight = c->getHeight();
    }

    for (int i = 0 ; i < grids.size() ; i++) {
        FieldGrid* grid = grids[i];
        if (grid->getWidth() > maxWidth)
          maxWidth = grid->getWidth();
        maxHeight += grid->getHeight();
    }

    for (int i = 0 ; i < footer.size() ; i++) {
        Component* c = footer[i];
        if (c->getWidth() > maxWidth)
          maxWidth = c->getWidth();
        if (c->getHeight() > maxHeight)
          maxHeight = c->getHeight();
    }

    return juce::Rectangle<int> {0, 0, maxWidth, maxHeight};
}

/**
 * Should only have one grid but if we have more than
 * one assume they stack.
 *
 * We often have a larger container when we're in a tabbed component
 * so center.
 */
void FormPanel::resized()
{
    juce::Rectangle<int> bounds = getMinimumSize();
    int contentHeight = bounds.getHeight();
    int contentOffset = (getHeight() - contentHeight) / 2;

    contentOffset = layoutComponents(&header, contentOffset);

    // sigh, can't pass OwnedArray<FieldGrid> to OwnedArray<Component>
    // should be a way to upcast these
    // I'd rather now change this to a Component array and make
    // everything else dynamic_cast down to FieldGrid
    for (int i = 0 ; i < grids.size() ; i++) {
        FieldGrid* grid = grids[i];
        int centerOffset = (getWidth() - grid->getWidth()) / 2;
        grid->setTopLeftPosition(centerOffset, contentOffset);
        contentOffset += grid->getHeight();
    }

    contentOffset = layoutComponents(&footer, contentOffset);
}

int FormPanel::layoutComponents(juce::OwnedArray<Component>* stuff, int rowOffset)
{
    for (int i = 0 ; i < stuff->size() ; i++) {
        Component* c = (*stuff)[i];
        int centerOffset = (getWidth() - c->getWidth()) / 2;
        c->setTopLeftPosition(centerOffset, rowOffset);
        rowOffset += c->getHeight();
    }
    return rowOffset;
}
    
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

        
    
