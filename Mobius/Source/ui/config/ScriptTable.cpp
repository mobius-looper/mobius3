
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../model/ScriptConfig.h"
#include "../../Supervisor.h"
#include "../common/ButtonBar.h"
#include "../JuceUtil.h"

#include "ScriptTable.h"

ScriptTable::ScriptTable(Supervisor* s)
{
    supervisor = s;
    setName("ScriptTable");

    initTable();
    addAndMakeVisible(table);

    commands.add("New");
    commands.add("Delete");
    commands.add("Move Up");
    commands.add("Move Down");
    commands.autoSize();
    commands.addListener(this);

    addAndMakeVisible(commands);
}

ScriptTable::~ScriptTable()
{
}

/**
 * Populate internal state with a list of Scripts from a ScriptConfig.
 */

void ScriptTable::setPaths(juce::StringArray paths)
{
    files.clear();

    for (auto path : paths) {
        ScriptTableFile* sf = new ScriptTableFile(path);
        files.add(sf);
        // color it red if it doesn't exist
        // this assumes that we have an absolute path, which should be the case now
        // don't think we need to support relative paths like we used to
        // note that unlike SampleTable we don't support $INSTALL prefixes
        // for script files, though that wouldn't be hard
        juce::File file (path);
        if (!file.existsAsFile() && !file.isDirectory())
          sf->missing = true;
    }
    table.updateContent();
}

void ScriptTable::updateContent()
{
    table.updateContent();
}

juce::StringArray ScriptTable::getResult()
{
    juce::StringArray paths;
    for (int i = 0 ; i < files.size() ; i++) {
        ScriptTableFile* sf = files[i];
        if (sf->path.length() > 0) {
            paths.add(sf->path);
        }
    }
    return paths;
}

/**
 * Delete contained Bindings and prepare for renewal.
 */
void ScriptTable::clear()
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
void ScriptTable::initTable()
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
void ScriptTable::initColumns()
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

int ScriptTable::getPreferredWidth()
{
    // todo: adapt to column configuration
    return 500;
}

int ScriptTable::getPreferredHeight()
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
void ScriptTable::resized()
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
void ScriptTable::buttonClicked(juce::String name)
{
    // is this the best way to compare them?
    if (name == juce::String("New")) {
        doFileChooser();
    }
    else if (name == juce::String("Delete")) {
        int row = table.getSelectedRow();
        if (row >= 0) {
            files.remove(row);
            table.updateContent();
            // auto-select the one after it?
        }
    }
    else if (name == juce::String("Move Up")) {
        int row = table.getSelectedRow();
        if (row >= 1) {
            ScriptTableFile* other = files[row-1];
            files.set(row-1, files[row], false);
            files.set(row, other, false);
            table.selectRow(row-1);
        }
    }
    else if (name == juce::String("Move Down")) {
        int row = table.getSelectedRow();
        if (row >= 0 && row < (files.size() - 1)) {
            ScriptTableFile* other = files[row+1];
            files.set(row+1, files[row], false);
            files.set(row, other, false);
            table.selectRow(row+1);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// TableListBoxModel
//
//////////////////////////////////////////////////////////////////////

juce::String ScriptTable::getCellText(int rowNumber, int columnId)
{
    (void)columnId;
    // only one column
    juce::String cell = files[rowNumber]->path;
    return cell;
}

/**
 * The maximum of all column rows.
 * This is independent of the table size.
 */
int ScriptTable::getNumRows()
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
void ScriptTable::paintRowBackground(juce::Graphics& g, int rowNumber,
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
void ScriptTable::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                             int width, int height, bool rowIsSelected)
{
    (void)columnId;
    ScriptTableFile* file = files[rowNumber];

    // what the tutorial did
    g.setColour (rowIsSelected ? juce::Colours::darkblue : getLookAndFeel().findColour (juce::ListBox::textColourId));

    // if we didn't find it, make it glow
    if (file->missing)
      g.setColour(juce::Colours::red);
    
    // how expensive is this, should we be caching it after the row height changes?
    g.setFont(JuceUtil::getFontf(height * .66f));

    // again from the table example
    // x, y, width, height, justification, useEllipses
    // example gave it 2 on the left, I guess to give it a little padding next to the cell border
    // same on the right with the width reduction
    // height was expected to be tall enough
    // centeredLeft means "centered vertically but placed on the left hand side"
    g.drawText (file->path, 2, 0, width - 4, height, juce::Justification::centredLeft, true);

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
void ScriptTable::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)rowNumber;
    (void)columnId;
    (void)event;
}

//////////////////////////////////////////////////////////////////////
//
// File Chooser
// Taken from the "Playing Sound Files" tutorial
//
//////////////////////////////////////////////////////////////////////

/**
 * Example is interesting because it actually reads the file
 * but all we want is the name.
 *
 * Trying to avoid modal dialogs where possible, so let's go
 * with the example and use launch_async
 *
 * launchAsync needs a "callback" which is declared as:
 *
 *   std::function< void(const FileChooser &) >
 * 
 * Don't have time right now to exhaustively understand this, but it's
 * basically a fancy function pointer.  I read this as a pointer to a function
 * returning void and taking a const FileChooser& argument.
 *
 * In this example it is implemented with a lamba expression which I also
 * don't have time for but it's basially a dynamiclly managed anonymous fnction.
 *
 * [this] defines the "closure" around the function which makes whatever the
 * contents are accessible to the body of the function.
 * (const  juce::FileChooser& fc) is the signature of the function and the
 * body is within braces as usual.
 *
 * With [this] you can reference object members as if you were in it, which
 * you are, but how you got there is magic.  Here we'll call ScriptTable's
 * fileChooserResult function.
 *
 * Observations: The window is huge, see if we can control that.
 * The dialog is modal in that it prevents you from interacting with
 * anything else, which is how many dialogs work but I don't like it.
 * The default starting path is c:/
 *
 * The second arg to the FileChooser constructor is "initialFileOrDirectory"
 * the example had juce::File() which I guess just makes it go to the root.
 * Third arg is the patterns to allow in the file extension.
 *
 * Fourth arg is useOSNativeDialogBox which defaults to true.
 * Then treatFilePackagesAsDirectories=false, not sure what that means.
 * And finally parentComponent=nullptr, if not set it opens a "top level window"
 * with some confusing words about AUv3s on iOS needing to specify this parameter
 * as "opening a top-level window in AUv3 is forbidden due to sandbox restrictions".
 * Explore that someday since we'll want to support AU and probably v3.
 * 
 */
void ScriptTable::doFileChooser()
{
    juce::File startPath(supervisor->getRoot());
    if (lastFolder.length() > 0)
      startPath = lastFolder;
    
    // a form of smart pointer
    chooser = std::make_unique<juce::FileChooser> ("Select a Script file ...",
                                                   startPath,
                                                   "*.mos;*.msl");

    // not documented under FileChooser, have to look
    // at FileBrowserComponent
    // adding canSelectDirectories forces it into a tree view rather than
    // the more native looking dialog that I think also supports file preview
    // you can select mulitple diretories, but only if they are peers
    // within the same parent directory, descending into one cancels the previous
    // selections
    auto chooserFlags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::canSelectMultipleItems
        | juce::FileBrowserComponent::canSelectDirectories;
    
    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        // magically get here after the modal dialog closes
        // the array will be empty if Cancel was selected
        juce::Array<juce::File> result = fc.getResults();
        if (result.size() > 0) {
            //Trace(2, "FileChooser chose these files:\n");
            for (auto file : result) {

                juce::String path = file.getFullPathName();
                //Trace(2, "  %s\n", path.toUTF8());

                ScriptTableFile* sf = new ScriptTableFile(path);
                files.add(sf);

                // remember this directory for the next time
                lastFolder = file.getParentDirectory().getFullPathName();
            }
            
            table.updateContent();
            // select it, it will be the last
            table.selectRow(files.size() - 1);
        }
        
    });
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

