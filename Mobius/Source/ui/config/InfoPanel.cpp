
#include <JuceHeader.h>

#include "../../util/MidiUtil.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Binding.h"
#include "../../Supervisor.h"
#include "InfoPanel.h"

const int InfoPanelFooterHeight = 20;

InfoPanel::InfoPanel()
{
    initTable();
    addAndMakeVisible(table);

    okButton.addListener(this);

    addAndMakeVisible(footer);
    footer.addAndMakeVisible(okButton);

    setSize(300, 600);
}

InfoPanel::~InfoPanel()
{
}

void InfoPanel::show(bool doMidi)
{
    midi = doMidi;
    centerInParent();
    setVisible(true);

    things.clear();
    MobiusConfig* config = Supervisor::Instance->getMobiusConfig();
    BindingSet* bindingSet = config->getBindingSets();
    Binding* bindings = bindingSet->getBindings();
    while (bindings != nullptr) {
        if ((midi && bindings->isMidi()) ||
            (!midi && bindings->trigger == TriggerKey)) {
            things.add(bindings);
        }
        bindings = bindings->getNext();
    }
    table.updateContent();
}

void InfoPanel::buttonClicked(juce::Button* b)
{
    (void)b;
    setVisible(false);
}

void InfoPanel::resized()
{
    juce::Rectangle area = getLocalBounds();
    area.removeFromBottom(5);
    area.removeFromTop(5);
    area.removeFromLeft(5);
    area.removeFromRight(5);
    
    footer.setBounds(area.removeFromBottom(InfoPanelFooterHeight));
    okButton.setSize(60, InfoPanelFooterHeight);
    centerInParent(okButton);

    table.setBounds(area);
}

void InfoPanel::paint(juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    g.setColour(juce::Colours::white);
    g.drawRect(getLocalBounds(), 4);
}

int InfoPanel::centerLeft(juce::Component& c)
{
    return centerLeft(this, c);
}

int InfoPanel::centerLeft(juce::Component* container, juce::Component& c)
{
    return (container->getWidth() / 2) - (c.getWidth() / 2);
}

int InfoPanel::centerTop(juce::Component* container, juce::Component& c)
{
    return (container->getHeight() / 2) - (c.getHeight() / 2);
}

void InfoPanel::centerInParent(juce::Component& c)
{
    juce::Component* parent = c.getParentComponent();
    c.setTopLeftPosition(centerLeft(parent, c), centerTop(parent, c));
}    

void InfoPanel::centerInParent()
{
    centerInParent(*this);
}    

//////////////////////////////////////////////////////////////////////
//
// TableListBoxModel
//
//////////////////////////////////////////////////////////////////////

void InfoPanel::initTable()
{
    table.setColour (juce::ListBox::outlineColourId, juce::Colours::grey);
    table.setOutlineThickness (1);
    table.setMultipleSelectionEnabled (false);
    table.setClickingTogglesRowSelection(true);
    table.setHeaderHeight(22);
    table.setRowHeight(22);

    initColumns();

}
        
void InfoPanel::initColumns()
{
    juce::TableHeaderComponent& header = table.getHeader();

    int triggerColumn = 1;
    int targetColumn = 2;

    // columnId, width, minWidth, maxWidth, propertyFlags, insertIndex
    // minWidth defaults to 30
    // maxWidth to -1
    // propertyFlags=defaultFlags
    // insertIndex=-1
    // propertyFlags has various options for visibility, sorting, resizing, dragging
    // example used 1 based column ids, is that necessary?

    header.addColumn(juce::String("Key"), triggerColumn,
                     100, 30, -1,
                     juce::TableHeaderComponent::defaultFlags);

    header.addColumn(juce::String("Target"), targetColumn,
                     100, 30, -1,
                     juce::TableHeaderComponent::defaultFlags);
}

/**
 * Callback from TableListBoxModel to derive the text to paint in this cell.
 * Row is zero based columnId is 1 based and is NOT a column index, you have
 * to map it to the logical column if allowing column reording in the table.
 *
 */
juce::String InfoPanel::getCellText(int row, int columnId)
{
    juce::String cell;

    Binding* b = things[row];
    if (columnId == 1) {
        if (midi) {
            cell = renderMidiTrigger(b);
        }
        else {
            // not currently storing modifiers in the Binding
            cell = KeyTracker::getKeyText(b->triggerValue, 0);
        }
    }
    else {
        cell = juce::String(b->getSymbolName());
    }
    
    return cell;
}

// need a MidiUtil for this
juce::String InfoPanel::renderMidiTrigger(Binding* b)
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
int InfoPanel::getNumRows()
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
void InfoPanel::paintRowBackground(juce::Graphics& g, int rowNumber,
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
void InfoPanel::paintCell(juce::Graphics& g, int rowNumber, int columnId,
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
void InfoPanel::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)rowNumber;
    (void)columnId;
    (void)event;
}
