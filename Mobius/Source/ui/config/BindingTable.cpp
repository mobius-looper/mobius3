
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../util/MidiUtil.h"
#include "../../model/old/OldBinding.h"
#include "../common/ButtonBar.h"
#include "../JuceUtil.h"

#include "BindingTable.h"

// should put this inside the class but we've got
// the "static member with in-class initializer must have non-volatile
// const integral type or be line" folderol to figure out
const char* NewBindingName = "[New]";

BindingTable::BindingTable()
{
    setName("BindingTable");

    initTable();
    addAndMakeVisible(table);

    commands.add("New");
    //commands.add("Update");
    commands.add("Copy");
    commands.add("Delete");
    commands.autoSize();
    commands.addListener(this);

    addAndMakeVisible(commands);
}

BindingTable::~BindingTable()
{
}

/**
 * Until we can get drag and drop worked out, hack in some
 * up/down buttons if you want ordering.  Can't be turned off
 * once set.
 */
void BindingTable::setOrdered(bool b)
{
    ordered = b;
    if (ordered) {
        commands.add("Move Up");
        commands.add("Move Down");
        commands.autoSize();
    }
}

/**
 * Populate internal state with a list of Bindings
 * from a configuration object.
 * The list is copied and ownership is retained by the caller.
 */

void BindingTable::setBindings(class OldBinding* src)
{
    bindings.clear();
    while (src != nullptr) {
        OldBinding* copy = new OldBinding(src);
        bindings.add(copy);
    }

    // should only be doing this once but I suppose we could
    // trigger a repaint if it comes in later
    table.updateContent();
}

void BindingTable::add(OldBinding* src)
{
    bindings.add(new OldBinding(src));
}

void BindingTable::updateContent()
{
    table.updateContent();
    // hmm, this isn't doing a refresh when called after BindingEditor
    // makes changes to one of the Bindings, the model changed
    // but you won't see it until you click on another row to change
    // the selection, weird, feels like we shouldn't have to do this
    repaint();
}

/**
 * Returns the list of Bindings that have been modified
 * and clears internal state.  Ownership of the list passes
 * to the caller.
 */
OldBinding* BindingTable::captureBindings()
{
    OldBinding* capture = nullptr;
    OldBinding* last = nullptr;

    //trace("BindingTable::capture %d\n", bindings.size());

    while (bindings.size() > 0) {
        OldBinding* b = bindings.removeAndReturn(0);
        // clearing lingering chain pointer for cascaded delete
        b->setNext(nullptr);
        const char* name = b->getSymbolName();
        //trace("%s\n", name);
        // filter out uninitialized rows
        if (StringEqual(name, NewBindingName)) {
            //trace("filtering %s\n", name);
            delete b;
        }
        else {
            if (last == nullptr)
              capture = b;
            else
              last->setNext(b);
            last = b;
        }
    }
    
    table.updateContent();

    return capture;
}

/**
 * Delete contained Bindings and prepare for renewal.
 */
void BindingTable::clear()
{
    bindings.clear();
    table.updateContent();
}

bool BindingTable::isNew(OldBinding* b)
{
    return StringEqual(b->getSymbolName(), NewBindingName);
}

void BindingTable::deselect()
{
    // easier to use deselectAllRows?
    int row = table.getSelectedRow();
    if (row >= 0) {
        table.deselectRow(row);
        if (listener != nullptr)
          listener->bindingDeselected();
    }
}

OldBinding* BindingTable::getSelectedBinding()
{
    OldBinding* binding = nullptr;
    int row = table.getSelectedRow();
    if (row >= 0) {
        binding = bindings[row];
    }
    return binding;
}

//////////////////////////////////////////////////////////////////////
//
// Layout
//
//////////////////////////////////////////////////////////////////////

/**
 * Remove the trigger column.
 * As currently organized, we don't have a way to set a flag during construction
 * to prevent the column from being added since initTable is called in
 * the constructor.
 */
void BindingTable::removeTrigger()
{
    juce::TableHeaderComponent& header = table.getHeader();
    header.removeColumn(TriggerColumn);
}

void BindingTable::addDisplayName()
{
    juce::TableHeaderComponent& header = table.getHeader();
    header.addColumn(juce::String("Display Name"), DisplayNameColumn,
                     100, 30, -1,
                     juce::TableHeaderComponent::defaultFlags);
}

/**
 * Set starting table properties
 */
void BindingTable::initTable()
{
    // from the example
    table.setColour (juce::ListBox::outlineColourId, juce::Colours::grey);      // [2]
    table.setOutlineThickness (1);

    // usually want this off but I guess could support for multiple deletes?
    table.setMultipleSelectionEnabled (false);
    // any reason not to want this?
    // only relevant if multi selection enabled
    // docs say to get toggle clicking in single-select mode use CMD or CTRL
    // when clicking, but that didn't work for me
    table.setClickingTogglesRowSelection(true);

    // unclear what the defaults are here, 
    // The default row height from ListBox is 22
    // Interesting, I don't think rows will squish based on the
    // overall table size.  Unless I guess if you change them
    // in response to resized() 
    table.setHeaderHeight(22);
    table.setRowHeight(22);

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
void BindingTable::initColumns()
{
    // take sorting out of the default flags until we can implement it correctly
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

    header.addColumn(juce::String("Target"), TargetColumn,
                     100, 30, -1,
                     columnFlags);
                     
    // trigger is optional for buttons
    header.addColumn(juce::String("Trigger"), TriggerColumn,
                     100, 30, -1,
                     columnFlags);

    header.addColumn(juce::String("Arguments"), ArgumentsColumn,
                     100, 30, -1,
                     columnFlags);
    
    header.addColumn(juce::String("Scope"), ScopeColumn,
                     50, 30, -1,
                     columnFlags);

    //header.setSortColumnId (1, true);

    // unclear what the default header height is
    // the default row height from ListBox is 22
}

const int CommandButtonGap = 10;

int BindingTable::getPreferredWidth()
{
    // adapt to column configuration
    return 500;
}

int BindingTable::getPreferredHeight()
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
void BindingTable::resized()
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
 *
 */
void BindingTable::buttonClicked(juce::String name)
{
    // is this the best way to compare them?
    if (name == juce::String("New")) {
        if (listener != nullptr) {
            OldBinding* neu = listener->bindingNew();

            // formerly treated returning null as meaning
            // to create a placeholder which could be
            // updated later, don't do that, require that
            // they have something selected to start with
            // so we can get rid of the Update button
#if 0            
            if (neu == nullptr) {
                // generate a placeholder
                neu = new OldBinding();
                neu->setSymbolName(NewBindingName);
            }
#endif
            if (neu != nullptr) {
                bindings.add(neu);
                table.updateContent();
                // select it, it will be the last
                // hmm, not sure about this, if this is left selected then if you
                // immediately pick a different target it modifies the new binding
                // seems better to have to re-select the one you want to modify
                //table.selectRow(bindings.size() - 1);
                table.scrollToEnsureRowIsOnscreen(bindings.size() - 1);
                // this actually won't do anything since New is normally clicked
                // when there is already nothing selected, if you want the target table
                // to always deselect must call the listener->bindingDeselected
                // kind of like having it remain in place though
                deselect();
            }
        }
    }
    else if (name == juce::String("Copy")) {
        int row = table.getSelectedRow();
        if (row >= 0) {
            OldBinding* b = bindings[row];
            if (listener != nullptr) {
                OldBinding* neu = listener->bindingCopy(b);
                if (neu != nullptr) {
                    bindings.add(neu);
                    table.updateContent();
                    // select it, it will be the last
                    table.selectRow(bindings.size() - 1);
                }
            }
        }
        
    }
    else if (name == juce::String("Update")) {
        // shouldn't get here any more now that we have immediate form capture
        int row = table.getSelectedRow();
        if (row >= 0) {
            OldBinding* b = bindings[row];
            if (listener != nullptr) {
                // listener updates the binding but we retain ownership
                listener->bindingUpdate(b);
                table.updateContent();
                // had to add this, why?  I guess just changing the
                // model without altering the row count doesn't trigger it
                table.repaint();
            }
        }
    }
    else if (name == juce::String("Delete")) {
        int row = table.getSelectedRow();
        if (row >= 0) {
            OldBinding* b = bindings[row];
            if (listener != nullptr) {
                // listener is allowed to respond, but it does not take
                // ownership of the Binding
                listener->bindingDelete(b);
            }
            bindings.remove(row);
            table.updateContent();
            // auto-select the one after it?
        }
    }
    else if (name == juce::String("Move Up")) {
        int row = table.getSelectedRow();
        if (row > 0) {
            bindings.swap(row, row-1);
            table.selectRow(row-1);
            table.updateContent();
            // weirdly updateContent wasn't enough?
            // have to repaint the whole thing, I guess because
            // the row count didn't change
            repaint();
        }
    }
    else if (name == juce::String("Move Down")) {
        int row = table.getSelectedRow();
        if (row < bindings.size() - 1) {
            bindings.swap(row, row+1);
            table.selectRow(row+1);
            table.updateContent();
            repaint();
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Table Cell Rendering
//
//////////////////////////////////////////////////////////////////////

/**
 * Callback from TableListBoxModel to derive the text to paint in this cell.
 * Row is zero based columnId is 1 based and is NOT a column index, you have
 * to map it to the logical column if allowing column reording in the table.
 *
 */
juce::String BindingTable::getCellText(int row, int columnId)
{
    juce::String cell;
    OldBinding* b = bindings[row];
    
    if (columnId == TargetColumn) {
        cell = juce::String(b->getSymbolName());
    }
    else if (columnId == TriggerColumn) {
        if (listener != nullptr)
          cell = listener->renderTriggerCell(b);
        else
          cell = "???";
    }
    else if (columnId == ScopeColumn) {
        // BindingEditor should probably render this
        cell = formatScopeText(b);
    }
    else if (columnId == ArgumentsColumn) {
        cell = juce::String(b->getArguments());
    }
    else if (columnId == DisplayNameColumn) {
        cell = b->displayName;
    }

    return cell;
}

/**
 * I think the old way stored these as text and they were
 * parsed at runtime into the mTrack and mGroup numbers
 * Need a lot more here as we refine what scopes mean.
 */ 
juce::String BindingTable::formatScopeText(OldBinding* b)
{
    if (b->getScope() == nullptr)
      return juce::String("Global");
    else
      return juce::String(b->getScope());
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
int BindingTable::getNumRows()
{
    return bindings.size();
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
void BindingTable::paintRowBackground(juce::Graphics& g, int rowNumber,
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
void BindingTable::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                             int width, int height, bool rowIsSelected)
{
    g.setColour (rowIsSelected ? juce::Colours::darkblue : getLookAndFeel().findColour (juce::ListBox::textColourId));
    
    // how expensive is this, should we be caching it after the row height changes?
    g.setFont(JuceUtil::getFontf(height * .66f));

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
void BindingTable::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)columnId;
    (void)event;

    if (rowNumber != lastSelection) {
        if (listener != nullptr) {
            if (rowNumber >= bindings.size()) {
                // trace("Binding row out of range! %d\n", rowNumber);
            }
            else {
                OldBinding* b = bindings[rowNumber];
                listener->bindingSelected(b);
            }
        }
        lastSelection = rowNumber;
    }
    else {
        // couldn't get cmd-click to work as documented, fake it
        deselect();
        lastSelection = -1;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

