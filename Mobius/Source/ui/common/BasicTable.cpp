/**
 * Extension of TableListBox that has the common options I use.
 * 
 * Includes support for columns with checkboxes based on this tutorial:
 * https://docs.juce.com/master/tutorial_table_list_box.html
 * 
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "BasicTable.h"

BasicTable::BasicTable()
{
    setColour (juce::ListBox::outlineColourId, juce::Colours::grey);
    setOutlineThickness (1);
    setMultipleSelectionEnabled (false);
    setClickingTogglesRowSelection(true);
    setHeaderHeight(22);
    setRowHeight(22);
    setModel(this);
}

void BasicTable::setBasicModel(Model* m)
{
    model = m;
}

void BasicTable::addColumn(juce::String name, int id, int width)
{
    juce::TableHeaderComponent& tableHeader = getHeader();

    // default includes visible, resizable, draggable, appearsOnColumnMenu, sortable
    // sortable is not relevant for most tables and causes confusion when things don't sort
    // appearsOnColumnMenu means "the columnn will be shown on the pop-up menu allowing it to
    // be hidden/shown, not sure what that means but I don't need it
    int columnFlags = juce::TableHeaderComponent::ColumnPropertyFlags::visible |
        juce::TableHeaderComponent::ColumnPropertyFlags::resizable |
        juce::TableHeaderComponent::ColumnPropertyFlags::draggable;

    // columnId, width, minWidth, maxWidth, propertyFlags, insertIndex
    // minWidth defaults to 30
    // maxWidth to -1
    // propertyFlags=defaultFlags
    // insertIndex=-1
    // propertyFlags has various options for visibility, sorting, resizing, dragging
    // example used 1 based column ids, is that necessary?

    tableHeader.addColumn(name, id, width, 30, -1, columnFlags);
    setCheckboxFlag(id, false);
}

void BasicTable::addColumnCheckbox(juce::String name, int id)
{
    juce::TableHeaderComponent& tableHeader = getHeader();
    int columnFlags = juce::TableHeaderComponent::ColumnPropertyFlags::visible |
        juce::TableHeaderComponent::ColumnPropertyFlags::resizable |
        juce::TableHeaderComponent::ColumnPropertyFlags::draggable;

    tableHeader.addColumn(name, id, 100, 30, -1, columnFlags);
    setCheckboxFlag(id, true);
}

/**
 * Usual annoyance of Juce and sparse arrays.
 * Array doesn't grow if the indecies happen to come in out of order
 * so always have to check capacity as we add.
 */
void BasicTable::setCheckboxFlag(int columnId, bool flag)
{
    while (checkboxColumns.size() < (columnId + 1))
      checkboxColumns.add(false);
    checkboxColumns.set(columnId, flag);
}

int BasicTable::getNumRows()
{
    return (model != nullptr) ? model->getNumRows() : 10;
}

juce::String BasicTable::getCellText(int row, int columnId)
{
    return (model != nullptr) ? model->getCellText(row, columnId) : "";
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
void BasicTable::paintRowBackground(juce::Graphics& g, int rowNumber,
                                    int width, int height,
                                    bool rowIsSelected)
{
    (void)width;
    (void)height;
    
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
 *
 * todo: I've carried around this analysis over multiple classes, get rid of it
 */
void BasicTable::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                             int width, int height, bool rowIsSelected)
{
    g.setColour (rowIsSelected ? juce::Colours::darkblue : getLookAndFeel().findColour (juce::ListBox::textColourId));
    
    // how expensive is this, should we be caching it after the row height changes?
    g.setFont(juce::Font(height * .66f));

    juce::String cell = getCellText(rowNumber, columnId);

    // again from the table example
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

/**
 * MouseEvent has various characters of the mouse click such as the actual x/y coordinate
 * offsetFromDragStart, numberOfClicks, etc.  Not interested in those right now.
 *
 * Can pass the row/col to the listener.
 * Can use ListBox::isRowSelected to get the selected row
 * Don't know if there is tracking of a selected column but we don't need that yet.
 */
void BasicTable::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)rowNumber;
    (void)columnId;
    (void)event;
}

//////////////////////////////////////////////////////////////////////
//
// Checkboxes
//
//////////////////////////////////////////////////////////////////////

/**
 * This is called for each cell to see if it needs a custom component.
 * If this row/column is supposed to have a checkbox, make one and return it.
 * Unclear why it would call this if it already has a custom component,
 * perhaps to change the row number due to sorting or dragging?
 * 
 */
juce::Component* BasicTable::refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
                                                      juce::Component* existingComponentToUpdate)
{
    (void)isRowSelected;
    // if this cell doesn't need a custom component, return nullptr
    juce::Component* custom = nullptr;
    
    bool needs = needsCheckbox(rowNumber, columnId);
    if (needs) {
        BasicTableCheckbox* checkbox = static_cast<BasicTableCheckbox*> (existingComponentToUpdate);

        if (checkbox == nullptr)
          checkbox = new BasicTableCheckbox (*this);

        checkbox->setRowAndColumn (rowNumber, columnId);
        custom = checkbox;
    }
    else if (existingComponentToUpdate != nullptr) {
        // we didn't think this cell needed one, but the table
        // has one, something went wrong
        Trace(1, "BasicTable: Found a custom cell where it didn't belong\n");
    }

    return custom;
}

bool BasicTable::needsCheckbox(int row, int column)
{
    (void)row;
    return checkboxColumns[column];
}

/**
 * Called by BasicTableCheckbox to get the current state of whatever this
 * represents.
 */
bool BasicTable::getCheck(int row, int column)
{
    if (model != nullptr)
      return model->getCellCheck(row, column);
    return false;
}

/**
 * Called by BasicTableCheckbox to set the current state of whatever this
 * represents.
 */
void BasicTable::doCheck(int row, int column, bool state)
{
    if (model != nullptr)
      model->setCellCheck(row, column, state);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
