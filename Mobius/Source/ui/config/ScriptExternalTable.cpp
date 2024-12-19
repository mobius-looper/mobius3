
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../model/ScriptConfig.h"
#include "../../Supervisor.h"
#include "../MainWindow.h"
#include "../../script/ScriptClerk.h"
#include "../../script/ScriptRegistry.h"
#include "../common/ButtonBar.h"
#include "../JuceUtil.h"

#include "ScriptConfigEditor.h"
#include "ScriptExternalTable.h"

ScriptExternalTable::ScriptExternalTable(Supervisor* s, ScriptConfigEditor* sce)
{
    supervisor = s;
    parent = sce;
    setName("ScriptExternalTable");

    initTable();
    addAndMakeVisible(table);

    commands.add("Add External");
    commands.add("Remove External");
    commands.autoSize();
    commands.addListener(this);

    addAndMakeVisible(commands);
}

ScriptExternalTable::~ScriptExternalTable()
{
}

/**
 * Populate internal state from the ScriptRegistry::Machine::Externals list
 */
void ScriptExternalTable::load(ScriptRegistry* reg)
{
    files.clear();

    // formerly based this off the Machine::externals list
    // File needs to be kept in sync with that
    ScriptRegistry::Machine* machine = reg->getMachine();
    for (auto ext : machine->externals) {

        ScriptExternalTableFile* efile = new ScriptExternalTableFile();
        files.add(efile);
        
        efile->path = ext->path;
        juce::File f (ext->path);
        if (f.isDirectory()) {
            efile->type = "Folder";
        }
        else if (!f.existsAsFile()) {
            efile->missing = true;
            efile->type = "Missing";
        }
        else if (f.getFileExtension() == ".msl") {
            efile->type = "MSL";
        }
        else if (f.getFileExtension() == ".mos") {
            efile->type = "MOS";
        }
        else {
            efile->type = "Unknown";
        }
        
        // shows the enable/disable status
        // !! this isn't working right with folders and we don't have
        // if we make this oriented toward just the External paths,
        // then you could not enable/dissable individisual files in a folder
        // only the entire folder, the older Library Files table would allow this
        // but it doesn't show externals any more, not worth messing with since
        // I'm trying to move everyone toward the library folder
        efile->registryFile = machine->findFile(efile->path);
    }
    table.updateContent();
}

void ScriptExternalTable::updateContent()
{
    table.updateContent();
}

/**
 * Get the array of external paths in the table.
 * Some of these may be directories.
 */
juce::StringArray ScriptExternalTable::getPaths()
{
    juce::StringArray paths;
    for (int i = 0 ; i < files.size() ; i++) {
        ScriptExternalTableFile* sf = files[i];
        if (sf->path.length() > 0) {
            paths.add(sf->path);
        }
    }
    return paths;
}

/**
 * Delete contained Bindings and prepare for renewal.
 */
void ScriptExternalTable::clear()
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
void ScriptExternalTable::initTable()
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
void ScriptExternalTable::initColumns()
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

    header.addColumn(juce::String("File Path"), ColumnPath,  450, 30, -1, columnFlags);
    header.addColumn(juce::String("Type"), ColumnType,  80, 30, -1, columnFlags);
    header.addColumn(juce::String("Status"), ColumnStatus,  80, 30, -1, columnFlags);
}

const int CommandButtonGap = 10;

int ScriptExternalTable::getPreferredWidth()
{
    // todo: adapt to column configuration
    return 500;
}

int ScriptExternalTable::getPreferredHeight()
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
void ScriptExternalTable::resized()
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
void ScriptExternalTable::buttonClicked(juce::String name)
{
    // is this the best way to compare them?
    if (name == juce::String("Add External")) {
        doFileChooser();
    }
    else if (name == juce::String("Remove External")) {
        int row = table.getSelectedRow();
        if (row >= 0) {
            files.remove(row);
            table.updateContent();
            // auto-select the one after it?

            // notify the containing editor that a file was removed
            if (parent != nullptr)
              parent->scriptExternalTableChanged();
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// TableListBoxModel
//
//////////////////////////////////////////////////////////////////////

juce::String ScriptExternalTable::getCellText(int rowNumber, int columnId)
{
    juce::String cell;
    
    ScriptExternalTableFile* efile = files[rowNumber];
    ScriptRegistry::File* rfile = efile->registryFile;
    
    if (columnId == ColumnPath) {
        cell = efile->path;
    }
    else if (columnId == ColumnType) {
        cell = efile->type;
    }
    else if (columnId == ColumnStatus) {
        if (rfile != nullptr) {
            MslDetails* fdetails = rfile->getDetails();
        
            if (rfile->disabled)
              cell += "disabled ";
            else if (!rfile->old && (fdetails == nullptr || !fdetails->published))
              cell += "unloaded ";
          
            if (rfile->hasErrors())
              cell = "errors ";
        }
    }
    return cell;
}

/**
 * The maximum of all column rows.
 * This is independent of the table size.
 */
int ScriptExternalTable::getNumRows()
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
void ScriptExternalTable::paintRowBackground(juce::Graphics& g, int rowNumber,
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
void ScriptExternalTable::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                             int width, int height, bool rowIsSelected)
{
    (void)columnId;
    ScriptExternalTableFile* file = files[rowNumber];

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

    juce::String cell = getCellText(rowNumber, columnId);
    
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
void ScriptExternalTable::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)rowNumber;
    (void)columnId;
    (void)event;
}

/**
 * Like ScriptLibraryTable allow double click to bring up the editor.
 */
void ScriptExternalTable::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)rowNumber;
    (void)columnId;
    (void)event;
    
    ScriptExternalTableFile* tfile = files[rowNumber];
    ScriptClerk* clerk = supervisor->getScriptClerk();
    ScriptRegistry::File* file = clerk->findFile(tfile->path);
    if (file != nullptr)
      supervisor->getMainWindow()->editScript(file);
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
 * you are, but how you got there is magic.  Here we'll call ScriptExternalTable's
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
void ScriptExternalTable::doFileChooser()
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

                ScriptExternalTableFile* sf = new ScriptExternalTableFile();
                sf->path = path;
                files.add(sf);

                // remember this directory for the next time
                lastFolder = file.getParentDirectory().getFullPathName();
            }
            
            table.updateContent();
            // select it, it will be the last
            table.selectRow(files.size() - 1);

            if (parent != nullptr)
              parent->scriptExternalTableChanged();
        }
        
    });
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

