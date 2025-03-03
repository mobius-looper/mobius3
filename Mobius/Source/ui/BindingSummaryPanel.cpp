
#include <JuceHeader.h>

#include "../util/Util.h"
#include "../util/MidiUtil.h"
#include "../model/old/MobiusConfig.h"
#include "../model/old/Binding.h"
#include "../model/UIConfig.h"
#include "../Supervisor.h"
#include "JuceUtil.h"

#include "BindingSummaryPanel.h"

BindingSummary::BindingSummary(Supervisor* s)
{
    supervisor = s;
    initTable();
    addAndMakeVisible(table);
}

BindingSummary::~BindingSummary()
{
}

void BindingSummary::prepare(bool doMidi)
{
    midi = doMidi;
    things.clear();
    MobiusConfig* config = supervisor->getOldMobiusConfig();
    UIConfig* uiconfig = supervisor->getUIConfig();
    BindingSet* bindingSets = config->getBindingSets();

    // the first one is always added
    addBindings(bindingSets);

    // the rest are added if they are active, this only applies to MIDI
    // it would be more reliable if this were driven from what is actually
    // installed in Binderator, which may filter conflicts or do other
    // things
    bindingSets = bindingSets->getNextBindingSet();
    while (bindingSets != nullptr) {
        if (uiconfig->isActiveBindingSet(juce::String(bindingSets->getName())))
          addBindings(bindingSets);
        bindingSets = bindingSets->getNextBindingSet();
    }
    
    table.updateContent();
}

void BindingSummary::addBindings(BindingSet* set)
{
    Binding* bindings = set->getBindings();
    while (bindings != nullptr) {
        if ((midi && bindings->isMidi()) ||
            (!midi && bindings->trigger == TriggerKey)) {

            // just for this panel, set the source binding set
            // name so they can see where it came from
            bindings->setSource(set->getName());

            things.add(bindings);
        }
        bindings = bindings->getNext();
    }
}

void BindingSummary::resized()
{
    table.setBounds(getLocalBounds());
}

//////////////////////////////////////////////////////////////////////
//
// TableListBoxModel
//
//////////////////////////////////////////////////////////////////////

void BindingSummary::initTable()
{
    table.setColour (juce::ListBox::outlineColourId, juce::Colours::grey);
    table.setOutlineThickness (1);
    table.setMultipleSelectionEnabled (false);
    table.setClickingTogglesRowSelection(true);
    table.setHeaderHeight(22);
    table.setRowHeight(22);

    initColumns();

}

const int BindingSummaryTriggerColumn = 1;
const int BindingSummaryTargetColumn = 2;
const int BindingSummaryScopeColumn = 3;
const int BindingSummaryArgumentsColumn = 4;
const int BindingSummarySourceColumn = 5;
        
void BindingSummary::initColumns()
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

    header.addColumn(juce::String("Trigger"), BindingSummaryTriggerColumn,
                     100, 30, -1,
                     columnFlags);

    header.addColumn(juce::String("Target"), BindingSummaryTargetColumn,
                     200, 30, -1,
                     columnFlags);

    header.addColumn(juce::String("Scope"), BindingSummaryScopeColumn,
                     50, 30, -1,
                     columnFlags);

    header.addColumn(juce::String("Arguments"), BindingSummaryArgumentsColumn,
                     50, 30, -1,
                     columnFlags);

    header.addColumn(juce::String("Source"), BindingSummarySourceColumn,
                     200, 30, -1,
                     columnFlags);
}

/**
 * Callback from TableListBoxModel to derive the text to paint in this cell.
 * Row is zero based columnId is 1 based and is NOT a column index, you have
 * to map it to the logical column if allowing column reording in the table.
 *
 */
juce::String BindingSummary::getCellText(int row, int columnId)
{
    juce::String cell;

    Binding* b = things[row];
    if (columnId == BindingSummaryTriggerColumn) {
        if (midi) {
            cell = renderMidiTrigger(b);
        }
        else {
            // not currently storing modifiers in the Binding
            cell = KeyTracker::getKeyText(b->triggerValue, 0);
        }
    }
    else if (columnId == BindingSummaryTargetColumn) {
        cell = juce::String(b->getSymbolName());
    }
    else if (columnId == BindingSummaryScopeColumn) {
        cell = juce::String(b->getScope());
    }
    else if (columnId == BindingSummaryArgumentsColumn) {
        cell = juce::String(b->getArguments());
    }
    else if (columnId == BindingSummarySourceColumn) {
        cell = juce::String(b->getSource());
    }
    
    return cell;
}

// need a MidiUtil for this
juce::String BindingSummary::renderMidiTrigger(Binding* b)
{
    juce::String text;
    Trigger* trigger = b->trigger;
    
    if (trigger == TriggerNote) {
        // old utility
        char buf[32];
        MidiNoteName(b->triggerValue, buf);
        // fuck, really need to figure out the proper way to concatenate strings
        text += juce::String(b->midiChannel);
        text += ":";
        text += buf;
        // not interested in velocity
    }
    else if (trigger == TriggerProgram) {
        text += juce::String(b->midiChannel);
        text += ":";
        text += "Pgm ";
        text += juce::String(b->triggerValue);
    }
    else if (trigger == TriggerControl) {
        text += juce::String(b->midiChannel);
        text += ":";
        text += "CC ";
        text += juce::String(b->triggerValue);
    }
    else if (trigger == TriggerPitch) {
        // did anyone really use this?
        text += juce::String(b->midiChannel);
        text += ":";
        text += "Pitch ";
        text += juce::String(b->triggerValue);
    }
    return text;
}

/**
 * The maximum of all column rows.
 * This is independent of the table size.
 */
int BindingSummary::getNumRows()
{
    return things.size();
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
void BindingSummary::paintRowBackground(juce::Graphics& g, int rowNumber,
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
void BindingSummary::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                             int width, int height, bool rowIsSelected)
{
    g.setColour (rowIsSelected ? juce::Colours::darkblue : getLookAndFeel().findColour (juce::ListBox::textColourId));
    
    // how expensive is this, should we be caching it after the row height changes?
    g.setFont(juce::Font(JuceUtil::getFontf(height * .66f)));

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
void BindingSummary::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)rowNumber;
    (void)columnId;
    (void)event;
}
