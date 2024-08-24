/*
 * Provides a basic table with column headers and various
 * content and notification options.
 * 
 * TableListBox has a method for setting header hight.
 * Row height is set through the inherited ListBox::setRowHeight
 * from the docs "The default height is 22 pixels"
 *
 * The example does not set rowHeight, the cell font is hard coded as
 *     juce::Font font           { 14.0f };
 *
 * Which it must know in order to call getStringWidth for auto-sizing column widths
 *
 * TableListBoxModel::getColumnAutoSizeWidth can be overridded to provide
 * the maximum size required for all cell data
 *
 * I don't see a way to set the minimum and maximum widths of columns
 * After addColumn is called so if you want to do this in seperate methods
 * would have to re-add all the columns?
 *
 * Unclear if columnId needs to be 1 based like the examples.
 * Let's start with zero based and see if that works.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../JuceUtil.h"
#include "Panel.h"
#include "SimpleTable.h"

SimpleTable::SimpleTable()
{
    setName("SimpleTable");
    addAndMakeVisible(table);

    // unclear what the default header height is
    // the default row height from ListBox is 22, leave it

    // from the example
    table.setColour (juce::ListBox::outlineColourId, juce::Colours::grey);      // [2]
    table.setOutlineThickness (1);

    table.setMultipleSelectionEnabled (true);
    
    // any reason not to want this?
    // only relevant if multi selection enabled
    table.setClickingTogglesRowSelection(true);
}

SimpleTable::~SimpleTable()
{
}

/**
 * Set the column titles and default the widths.
 * Column Ids will be numbered starting from 1.
 * They must have a value greater than zero and must be unique.
 *
 * NOTE: Assuming this can only be called once, really
 * should clear the current header and start over if called again.
 */
void SimpleTable::setColumnTitles(juce::StringArray& titles)
{
    juce::TableHeaderComponent& header = table.getHeader();

    for (int i = 0 ; i < titles.size() ; i++) {
        // columnId, width, minWidth, maxWidth, propertyFlags, insertIndex
        // minWidth defaults to 30
        // maxWidth to -1
        // propertyFlags=defaultFlags
        // insertIndex=-1
        // propertyFlags has various options for visibility, sorting, resizing, dragging
        // example used 1 based column ids, is that necessary?
        header.addColumn(titles[i], i+1, 100, 30, -1,
                         juce::TableHeaderComponent::defaultFlags);
    }
    //header.setSortColumnId (1, true);

}

/**
 * Set the header height.
 * Unclear what the default header height is
 * The default row height from ListBox is 22.
 */
void SimpleTable::setHeaderHeight(int h)
{
    table.setHeaderHeight(h);
}

int SimpleTable::getHeaderHeight()
{
    return table.getHeaderHeight();
}

/**
 * Defaults to 22 in ListBox
 * Interesting, I don't think rows will squish based on the
 * overall table size.  Unless I guess if you change them
 * in response to resized() 
 */
void SimpleTable::setRowHeight(int h)
{
    table.setRowHeight(h);
}

int SimpleTable::getRowHeight()
{
    return table.getRowHeight();
}

/**
 * Set the width of a column.
 * This will be the initial size if the table allows resizing columns.
 * If set after the table is built, it would resize the column.
 */
void SimpleTable::setColumnWidth(int col, int width)
{
    juce::TableHeaderComponent& header = table.getHeader();

    // internal columnIds start with 1
    header.setColumnWidth(col+1, width);
}

/**
 * Set text for our cell data array.
 * Note that columns start from zero unlike TableModel columnIds
 *
 * juce::StringArray does not like leaving empty cells, if you call
 * set or insert and the index is greater than the size it is
 * simply appended.  To leave gaps you have to add empty strings.
 * Didn't see a built-in way to do this or a SparseStringArray.
 */
void SimpleTable::setCell(int row, int col, juce::String data)
{
    // note that the way this is defined
    // juce::Array<juce::StringArray> columns;
    // you won't get nullptr back from columns[col]
    // it seems to auto-create them as they are referenced?
    // this seems odd, OwnedArray is more obvious and may result
    // in less copying?
    // juce::StringArray& column = columns[col];
    // column.set(row, data);

    juce::StringArray* column = columns[col];
    if (column == nullptr) {
        column = new juce::StringArray();
        columns.set(col, column);
    }

    // pad to the desired row
    for (int i = column->size() ; i < row ; i++) {
        column->set(i, juce::String(""));
    }
    
    column->set(row, data);
}

int SimpleTable::getSelectedRow()
{
    return table.getSelectedRow();
}

/**
 * Not sure if this is something it tracks.
 * If not, we have to expose or track cellClicked
 */
int SimpleTable::getSelectedColumn()
{
    return 0;
}

void SimpleTable::dumpCells()
{
    for (int i = 0 ; i < columns.size() ; i++) {
        juce::StringArray* column = columns[i];
        if (column != nullptr) {
            for (int j = 0 ; j < column->size() ; j++) {
                juce::String cell = (*column)[j];
                if (cell.length() > 0) {
                    trace("Row %d col %d: %s\n", j, i, cell.toUTF8());
                }
            }
        }
    }
}

void SimpleTable::clear()
{
    columns.clear();
    table.deselectAllRows();
    table.updateContent();
}

void SimpleTable::updateContent()
{
    table.updateContent();
    // this alone isn't enough to get it to repaint for some reason
    table.repaint();
}

/**
 * Todo: don't have a way to specify multiple rows.
 */
void SimpleTable::setSelectedRow(int row)
{
    table.selectRow(row);
}

//////////////////////////////////////////////////////////////////////
//
// Component
//
//////////////////////////////////////////////////////////////////////

/**
 * Take what we get and leave column configuration alone for now.
 * Potentially could adjust header and row heights here
 * to make them fit better in smaller spaces.
 *
 * For testing, leave an inset with an undercolor so we can see
 * where bounds are.
 */
void SimpleTable::resized()
{
    table.setBounds(getLocalBounds());
}

void SimpleTable::paint (juce::Graphics& g)
{
    (void)g;
}

//////////////////////////////////////////////////////////////////////
//
// TableListBoxModel
//
//////////////////////////////////////////////////////////////////////

/**
 * This must be the maximum of all column rows.
 * This is independent of the table size.
 */
int SimpleTable::getNumRows()
{
    int maxRows = 0;

    for (int i = 0 ; i < columns.size() ; i++) {
        juce::StringArray* column = columns[i];
        if (column != nullptr) {
            int colRows = column->size();
            if (colRows > maxRows) maxRows = colRows;
        }
    }
    return maxRows;
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
void SimpleTable::paintRowBackground(juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/,
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

void SimpleTable::paintCell (juce::Graphics& g, int rowNumber, int columnId,
                             int width, int height, bool rowIsSelected)
{
    g.setColour (rowIsSelected ? juce::Colours::darkblue : getLookAndFeel().findColour (juce::ListBox::textColourId));
    
    // how expensive is this, should we be caching it after the row height changes?
    g.setFont(JuceUtil::getFontf(height * .66f));

    // now that columnId is 1 based
    juce::StringArray* column = columns[columnId-1];
    if (column != nullptr) {
        juce::String cell = (*column)[rowNumber];
    
        // x, y, width, height, justification, useEllipses
        // example gave it 2 on the left, I guess to give it a little padding next to the cell border
        // same on the right with the width reduction
        // height was expected to be tall enough
        // centeredLeft means "centered vertically but placed on the left hand side"
        g.drawText (cell, 2, 0, width - 4, height, juce::Justification::centredLeft, true);

        // this is odd, it fills a little rectangle on the right edge 1 pixel wide with
        // the background color, I'm guessing if the text is long enough, perhaps with elippses,
        // this erases part of to make it look better?
        //g.setColour (getLookAndFeel().findColour (juce::ListBox::backgroundColourId));
        //g.fillRect (width - 1, 0, 1, height);
    }
}

/**
 * MouseEvent has various characters of the mouse click such as the actual x/y coordinate
 * offsetFromDragStart, numberOfClicks, etc.  Not interested in those right now.
 *
 * Can pass the row/col to the listener.
 * Can use ListBox::isRowSelected to get the selected row
 * Don't know if there is tracking of a selected column but we don't need that yet.
 */
void SimpleTable::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)rowNumber;
    (void)columnId;
    (void)event;
    if (listener != nullptr)
      listener->tableTouched(this);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
