/**
 * The table of exported script symbols (linkages)
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../model/Symbol.h"
#include "../../model/ScriptProperties.h"
#include "../../Supervisor.h"
#include "../../Prompter.h"

#include "../common/ButtonBar.h"
#include "../JuceUtil.h"
#include "../MainWindow.h"

#include "../../script/ScriptRegistry.h"
#include "../../script/MslDetails.h"
#include "../../script/MslLinkage.h"
#include "../../script/MslCompilation.h"

//#include "ScriptFileDetails.h"
#include "ScriptSymbolTable.h"

ScriptSymbolTable::ScriptSymbolTable(Supervisor* s)
{
    supervisor = s;
    setName("ScriptSymbolTable");

    initTable();
    addAndMakeVisible(table);

    commands.add("Edit");
    commands.add("Details");
    commands.autoSize();
    commands.addListener(this);

    addAndMakeVisible(commands);

    //addChildComponent(details);
}

ScriptSymbolTable::~ScriptSymbolTable()
{
}

void ScriptSymbolTable::load(ScriptRegistry* reg)
{
    ScriptRegistry::Machine* machine = reg->getMachine();
    juce::File libdir = supervisor->getScriptClerk()->getLibraryFolder();
    symbols.clear();

    SymbolTable* stable = supervisor->getSymbols();
    for (auto symbol : stable->getSymbols()) {
        if (symbol->script != nullptr) {
            if (symbol->script->mslLinkage != nullptr || symbol->script->coreScript != nullptr) {
                ScriptSymbolTableRow* row = new ScriptSymbolTableRow();
                row->symbol = symbol;

                // cache the location so we don't have to do this every time the
                // cell is rendered
                ScriptRegistry::File* srf = machine->findFileByName(symbol->name);
                if (srf == nullptr && symbol->script->mslLinkage) {
                    // this can happen for exported functions that weren't the
                    // body function of the unit, the unit id is the full path
                    // except for scriptlets
                    MslLinkage* link = symbol->script->mslLinkage;
                    srf = machine->findFile(link->unit->id);
                }
                
                if (srf != nullptr) {
                    row->registryFile = srf;
                    juce::File f (srf->path);
                    if (srf->external != nullptr)
                      row->location = "External: " + f.getFileName();
                    else
                      row->location = f.getFileName();
                }
                else if (symbol->script->coreScript != nullptr) {
                    // File wasn't stored with the right name or something
                    Trace(1, "ScriptSymbolTable: Symbol for core script not in registry %s",
                          symbol->getName());
                    row->location = "???";
                }
                else {
                    // since we handled the unit id above, should only be here
                    // for scriptlets
                    MslLinkage* link = symbol->script->mslLinkage;
                    juce::File f (link->unit->id);
                    juce::File p = f.getParentDirectory();
                    if (p != libdir) {
                        // more to do here for scriptlets
                        row->location = link->unit->id;
                    }
                    else {
                        Trace(1, "ScriptSymbolTable: Symbol for MSL linkage not in registry %s",
                              symbol->getName());
                        row->location = "Library: " + f.getFileName();
                    }
                }
                    
                symbols.add(row);
            }
        }
    }
    
    table.updateContent();
}

void ScriptSymbolTable::updateContent()
{
    table.updateContent();
}

/**
 * Delete contained Bindings and prepare for renewal.
 */
void ScriptSymbolTable::clear()
{
    symbols.clear();
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
void ScriptSymbolTable::initTable()
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
void ScriptSymbolTable::initColumns()
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

    header.addColumn(juce::String("Name"), ColumnName,  200, 30, -1, columnFlags);
    header.addColumn(juce::String("Type"), ColumnType, 80, 30, -1, columnFlags);
    // too cluttery?
    header.addColumn(juce::String("Location"), ColumnLocation, 300, 30, -1, columnFlags);
}

const int CommandButtonGap = 10;

int ScriptSymbolTable::getPreferredWidth()
{
    // todo: adapt to column configuration
    return 500;
}

int ScriptSymbolTable::getPreferredHeight()
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
void ScriptSymbolTable::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    // leave some air between the two rows of buttons
    area.removeFromBottom(12);
    
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
void ScriptSymbolTable::buttonClicked(juce::String name)
{
    (void)name;
}

//////////////////////////////////////////////////////////////////////
//
// TableListBoxModel
//
//////////////////////////////////////////////////////////////////////

/**
 * For the status column there are some combinations:
 *
 * disabled
 *  This means the file will not have been loaded, HOWEVER
 *  it could be set after loading failed and there could
 *  be errors left behind that are interesting.  In that case
 *  the status could be "disabled/errors".  Really disabled
 *  should be a checkbox independent of the status
 *
 */
juce::String ScriptSymbolTable::getCellText(int rowNumber, int columnId)
{
    juce::String cell;
    
    ScriptSymbolTableRow* row = symbols[rowNumber];
    if (columnId == ColumnName) {
        cell = row->symbol->name;
    }
    else if (columnId == ColumnType) {
        MslLinkage* link = row->symbol->script->mslLinkage;
        if (link != nullptr) {
            if (link->isFunction)
              cell = "Function";
            else
              cell = "Variable";
        }
        else {
            // there was marginal support for variables but no one used them
            cell = "Function";
        }
    }
    else if (columnId == ColumnLocation) {
        cell = row->location;
    }

    return cell;
}

/**
 * The maximum of all column rows.
 * This is independent of the table size.
 */
int ScriptSymbolTable::getNumRows()
{
    return symbols.size();
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
void ScriptSymbolTable::paintRowBackground(juce::Graphics& g, int rowNumber,
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
void ScriptSymbolTable::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                             int width, int height, bool rowIsSelected)
{
    (void)columnId;
    //ScriptSymbolTableRow* row = symbols[rowNumber];

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
void ScriptSymbolTable::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)columnId;
    (void)event;
    
    ScriptSymbolTableRow* row = symbols[rowNumber];
    if (row->registryFile != nullptr)
      supervisor->getMainWindow()->editScript(row->registryFile);
}

/**
 * Alternative to cellClicked that picks up selection changes
 * when you use the arrow keys on the keyboard.
 */
void ScriptSymbolTable::selectedRowsChanged(int lastRowSelected)
{
    (void)lastRowSelected;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

