
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../model/ScriptConfig.h"
#include "../../Supervisor.h"
#include "../common/ButtonBar.h"
#include "../JuceUtil.h"
#include "../../script/ScriptRegistry.h"

#include "ScriptLibraryTable.h"

ScriptLibraryTable::ScriptLibraryTable(Supervisor* s)
{
    supervisor = s;
    setName("ScriptLibraryTable");

    initTable();
    addAndMakeVisible(table);

    commands.add("Enable");
    commands.add("Disable");
    commands.add("Details");
    commands.autoSize();
    commands.addListener(this);

    addAndMakeVisible(commands);
}

ScriptLibraryTable::~ScriptLibraryTable()
{
}

/**
 * Populate internal state with a list of Scripts from a ScriptConfig.
 */

void ScriptLibraryTable::load(ScriptRegistry* reg)
{
    files.clear();

    ScriptRegistry::Machine* machine = reg->getMachine();
    for (auto file : machine->files) {
        files.add(new ScriptLibraryTableFile(file));
    }

    table.updateContent();
}

void ScriptLibraryTable::updateContent()
{
    table.updateContent();
}

/**
 * Delete contained Bindings and prepare for renewal.
 */
void ScriptLibraryTable::clear()
{
    files.clear();
    table.updateContent();
}

//////////////////////////////////////////////////////////////////////
//
// Layout
//
//////////////////////////////////////////////////////////////////////

/**
 * Set starting table properties
 */
void ScriptLibraryTable::initTable()
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

    // the table is ordered, turn off sorting!!
    
    initColumns();
}

/**
 * Set the column titles and initial widths.
 * Column Ids must start from 1 and must be unique.
 *
 * NOTE: Assuming this can only be called once, really
 * should clear the current header and start over if called again.
 *
 * Pick some reasonable default widths but need to be smarter
 */
void ScriptLibraryTable::initColumns()
{
    // default includes visible, resizable, draggable, appearsOnColumnMenu, sortable
    // sortable is not relevant for most tables and causes confusion when things don't sort
    // todo: This is a table where sorting could be useful
    int columnFlags = juce::TableHeaderComponent::ColumnPropertyFlags::visible |
        juce::TableHeaderComponent::ColumnPropertyFlags::resizable |
        juce::TableHeaderComponent::ColumnPropertyFlags::draggable;
    
    juce::TableHeaderComponent& header = table.getHeader();

    fileColumn = 1;
    
    // columnId, width, minWidth, maxWidth, propertyFlags, insertIndex
    // minWidth defaults to 30
    // maxWidth to -1
    // propertyFlags=defaultFlags
    // insertIndex=-1
    // propertyFlags has various options for visibility, sorting, resizing, dragging
    // example used 1 based column ids, is that necessary?

    header.addColumn(juce::String("File"), fileColumn,
                     450, 30, -1,
                     columnFlags);
}

const int CommandButtonGap = 10;

int ScriptLibraryTable::getPreferredWidth()
{
    // todo: adapt to column configuration
    return 500;
}

int ScriptLibraryTable::getPreferredHeight()
{
    int height = 400;
    
    // gap between buttons
    height += CommandButtonGap;
    
    commands.autoSize();
    height += commands.getHeight();

    return height;
}

/**
 * Always put buttons at the bottom, and let the table
 * be as large as it wants.
 */
void ScriptLibraryTable::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    commands.setBounds(area.removeFromBottom(commands.getHeight()));
    area.removeFromBottom(CommandButtonGap);
 
    table.setBounds(area);
}    

//////////////////////////////////////////////////////////////////////
//
// Command Buttons
//
//////////////////////////////////////////////////////////////////////

/**
 * ButtonBar::Listener
 */
void ScriptLibraryTable::buttonClicked(juce::String name)
{
    // is this the best way to compare them?
    if (name == juce::String("Enable")) {
    }
    else if (name == juce::String("Disable")) {
    }
    else if (name == juce::String("Details")) {
    }
}

//////////////////////////////////////////////////////////////////////
//
// TableListBoxModel
//
//////////////////////////////////////////////////////////////////////

juce::String ScriptLibraryTable::getCellText(int rowNumber, int columnId)
{
    (void)columnId;
    // only one column
    //juce::String cell = files[rowNumber]->path;
    juce::String cell;
    
    return cell;
}

/**
 * The maximum of all column rows.
 * This is independent of the table size.
 */
int ScriptLibraryTable::getNumRows()
{
    return files.size();
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
void ScriptLibraryTable::paintRowBackground(juce::Graphics& g, int rowNumber,
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
void ScriptLibraryTable::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                             int width, int height, bool rowIsSelected)
{
    (void)columnId;
    ScriptLibraryTableFile* file = files[rowNumber];

    // what the tutorial did
    g.setColour (rowIsSelected ? juce::Colours::darkblue : getLookAndFeel().findColour (juce::ListBox::textColourId));

    // if we didn't find it, make it glow
    //if (file->missing)
    //g.setColour(juce::Colours::red);
    
    // how expensive is this, should we be caching it after the row height changes?
    g.setFont(JuceUtil::getFontf(height * .66f));

    // again from the table example
    // x, y, width, height, justification, useEllipses
    // example gave it 2 on the left, I guess to give it a little padding next to the cell border
    // same on the right with the width reduction
    // height was expected to be tall enough
    // centeredLeft means "centered vertically but placed on the left hand side"
    
    g.drawText (file->file->path, 2, 0, width - 4, height, juce::Justification::centredLeft, true);

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
void ScriptLibraryTable::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)rowNumber;
    (void)columnId;
    (void)event;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

