
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"

#include "../common/ButtonBar.h"
#include "../JuceUtil.h"

#include "TypicalTable.h"

TypicalTable::TypicalTable()
{
    setName("TypicalTable");
}

TypicalTable::~TypicalTable()
{
}

void TypicalTable::initialize()
{
    initTable();
    addAndMakeVisible(table);
}    

void TypicalTable::updateContent()
{
    table.updateContent();
    
    // I'm having chronic problems with tables not refreshing if all you
    // do is call is call updateContent, not sure if it's something wrong
    // with my wrapper classes, or my misunderstanding of Juce tables
    // forcing a repaint seems to do the trick
    table.repaint();
}

void TypicalTable::addCommand(juce::String name)
{
    commands.add(name);
    if (!hasCommands) {
        addAndMakeVisible(commands);
        commands.addListener(this);
    }
    hasCommands = true;
    commands.autoSize();
}

int TypicalTable::getSelectedRow()
{
    return table.getSelectedRow();
}

void TypicalTable::selectRow(int n)
{
    table.selectRow(n);
}

//////////////////////////////////////////////////////////////////////
//
// Layout
//
//////////////////////////////////////////////////////////////////////

/**
 * Set starting table properties
 */
void TypicalTable::initTable()
{
    // from the example
    table.setColour (juce::ListBox::outlineColourId, juce::Colours::grey);      // [2]
    table.setOutlineThickness (1);

    // usually want this off but I guess could support for multiple deletes?
    table.setMultipleSelectionEnabled (false);
    // any reason not to want this?
    // only relevant if multi selection enabled
    table.setClickingTogglesRowSelection(true);

    // unclear what the defaults are here, 
    // The default row height from ListBox is 22
    // Interesting, I don't think rows will squish based on the
    // overall table size.  Unless I guess if you change them
    // in response to resized() 
    table.setHeaderHeight(22);
    table.setRowHeight(22);
}

/**
 * Set the column titles and initial widths.
 * Column Ids must start from 1 and must be unique.
 */
void TypicalTable::addColumn(juce::String name, int id, int width)
{
    // default includes visible, resizable, draggable, appearsOnColumnMenu, sortable
    // sortable is not relevant for most tables and causes confusion when things don't sort
    // todo: This is a table where sorting could be useful
    int columnFlags = juce::TableHeaderComponent::ColumnPropertyFlags::visible |
        juce::TableHeaderComponent::ColumnPropertyFlags::resizable |
        juce::TableHeaderComponent::ColumnPropertyFlags::draggable;
    
    juce::TableHeaderComponent& header = table.getHeader();

    // columnId, width, minWidth, maxWidth, propertyFlags, insertIndex
    // minWidth defaults to 30
    // maxWidth to -1
    // propertyFlags=defaultFlags
    // insertIndex=-1
    // propertyFlags has various options for visibility, sorting, resizing, dragging
    // example used 1 based column ids, is that necessary?

    header.addColumn(name, id, width, 30, -1, columnFlags);
}

const int CommandButtonGap = 10;

int TypicalTable::getPreferredWidth()
{
    // todo: adapt to column configuration
    return 500;
}

int TypicalTable::getPreferredHeight()
{
    int height = 400;

    if (hasCommands) {
        // gap between buttons
        height += CommandButtonGap;
    
        commands.autoSize();
        height += commands.getHeight();
    }
    
    return height;
}

/**
 * Always put buttons at the bottom, and let the table
 * be as large as it wants.
 */
void TypicalTable::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    if (hasCommands) {
        // leave some air between the two rows of buttons
        area.removeFromBottom(12);
    
        commands.setBounds(area.removeFromBottom(commands.getHeight()));
        area.removeFromBottom(CommandButtonGap);
    }
    
    table.setBounds(area);
}    

/**
 * ButtonBar::Listener
 */
void TypicalTable::buttonClicked(juce::String name)
{
    doCommand(name);
}

//////////////////////////////////////////////////////////////////////
//
// TableListBoxModel
//
//////////////////////////////////////////////////////////////////////

/**
 * The maximum of all column rows.
 * This is independent of the table size.
 */
int TypicalTable::getNumRows()
{
    return getRowCount();
}

/**
 * Taken from the example to show alternate row backgrounds.
 * Colors look reasonable, don't really need to mess with
 * LookAndFeel though.
 *
 * Graphics will be initialized to the size of the visible row.
 * Width and height are passed, I guess in case you want to do something
 * fancier than just filling the entire thing.  Could be useful
 * for borders, though Juce might provide something for selected rows/cells already.
 */
void TypicalTable::paintRowBackground(juce::Graphics& g, int rowNumber,
                                      int /*width*/, int /*height*/,
                                      bool rowIsSelected)
{
    // I guess this makes an alternate color that is a variant of the existing background
    // color rather than having a hard coded unrelated color
    auto alternateColour = getLookAndFeel().findColour (juce::ListBox::backgroundColourId)
        .interpolatedWith (getLookAndFeel().findColour (juce::ListBox::textColourId), 0.03f);

    if (rowIsSelected) {
        g.fillAll (juce::Colours::lightblue);
    }
    else if (rowNumber % 2) {
        g.fillAll (alternateColour);
    }
}

/**
 * Based on the example.
 * If the row is selected it will have a light blue backgound and we'll paint the
 * text in dark blue.  Otherwise we use whatever the text color is set in the ListBox
 * 
 * Example had font hard coded as Font(14.0f) which is fine if you let the row heiught
 * default to 22 but ideally this should be proportional to the row height if it can be changed.
 * 14 is 63% of 22
 */
void TypicalTable::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                             int width, int height, bool rowIsSelected)
{
    (void)columnId;
    //TypicalTableRow* row = symbols[rowNumber];

    // what the tutorial did
    g.setColour (rowIsSelected ? juce::Colours::darkblue : getLookAndFeel().findColour (juce::ListBox::textColourId));

    // highlight errors
    //if (columnId == ColumnStatus && file->hasErrors())
    //g.setColour(juce::Colours::red);
    
    // how expensive is this, should we be caching it after the row height changes?
    g.setFont(JuceUtil::getFontf(height * .66f));

    // again from the table example
    // x, y, width, height, justification, useEllipses
    // example gave it 2 on the left, I guess to give it a little padding next to the cell border
    // same on the right with the width reduction
    // height was expected to be tall enough
    // centeredLeft means "centered vertically but placed on the left hand side"

    juce::String cell = getCellText(rowNumber, columnId);
    
    g.drawText (cell, 2, 0, width - 4, height, juce::Justification::centredLeft, true);

    // this is odd, it fills a little rectangle on the right edge 1 pixel wide with
    // the background color, I'm guessing if the text is long enough, perhaps with elippses,
    // this erases part of to make it look better?
    //g.setColour (getLookAndFeel().findColour (juce::ListBox::backgroundColourId));
    //g.fillRect (width - 1, 0, 1, height);
}

/**
 * Chicken and egg here.
 * selectedRowsChanged will be called first, then this one.
 * So when this starts invisible, selectedRowsChanged won't have done anything
 * and we have to both make it visible and refresh the contents.
 *
 * If it is already visible we don't have to do anything. 
 */
void TypicalTable::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)rowNumber;
    (void)columnId;
    (void)event;
}

/**
 * Alternative to cellClicked that picks up selection changes
 * when you use the arrow keys on the keyboard.
 */
void TypicalTable::selectedRowsChanged(int lastRowSelected)
{
    (void)lastRowSelected;
    if (listener != nullptr)
      listener->typicalTableChanged(this, table.getSelectedRow());
}

/**
 * MouseEvent has various characters of the mouse click such as the actual x/y coordinate
 * offsetFromDragStart, numberOfClicks, etc.  Not interested in those right now.
 *
 * Can pass the row/col to the listener.
 * Can use ListBox::isRowSelected to get the selected row
 * Don't know if there is tracking of a selected column but we don't need that yet.
 */
void TypicalTable::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)rowNumber;
    (void)columnId;
    (void)event;
    if (listener != nullptr) 
      listener->typicalTableClicked(this, table.getSelectedRow());
}


/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
