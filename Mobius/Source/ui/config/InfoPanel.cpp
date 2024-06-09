
#include <JuceHeader.h>

#include "../../util/MidiUtil.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Binding.h"
#include "../JuceUtil.h"
#include "../../Supervisor.h"
#include "InfoPanel.h"

const int InfoPanelHeaderHeight = 20;
const int InfoPanelFooterHeight = 20;

InfoPanel::InfoPanel()
{
    addAndMakeVisible(resizer);
    resizer.setBorderThickness(juce::BorderSize<int>(4));
    resizeConstrainer.setMinimumHeight(20);
    resizeConstrainer.setMinimumWidth(20);

    initTable();
    addAndMakeVisible(table);

    okButton.addListener(this);

    addAndMakeVisible(footer);
    footer.addAndMakeVisible(okButton);

    setSize(600, 600);
}

InfoPanel::~InfoPanel()
{
}

void InfoPanel::show(bool doMidi)
{
    midi = doMidi;
    JuceUtil::centerInParent(this);
    setVisible(true);

    things.clear();
    MobiusConfig* config = Supervisor::Instance->getMobiusConfig();
    BindingSet* bindingSets = config->getBindingSets();

    // the first one is always added
    addBindings(bindingSets);

    // the rest are added if they are active, this only applies to MIDI
    // it would be more reliable if this were driven from what is actually
    // installed in Binderator, which may filter conflicts or do other
    // things
    bindingSets = bindingSets->getNextBindingSet();
    while (bindingSets != nullptr) {
        if (bindingSets->isActive())
          addBindings(bindingSets);
        bindingSets = bindingSets->getNextBindingSet();
    }
    
    table.updateContent();
}

void InfoPanel::addBindings(BindingSet* set)
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

void InfoPanel::buttonClicked(juce::Button* b)
{
    (void)b;
    setVisible(false);
}

void InfoPanel::resized()
{
    juce::Rectangle area = getLocalBounds();

    resizer.setBounds(area);

    area.removeFromTop(InfoPanelHeaderHeight);
    
    area.removeFromBottom(5);
    area.removeFromTop(5);
    area.removeFromLeft(5);
    area.removeFromRight(5);
    
    footer.setBounds(area.removeFromBottom(InfoPanelFooterHeight));
    okButton.setSize(60, InfoPanelFooterHeight);
    JuceUtil::centerInParent(&okButton);

    table.setBounds(area);
}

void InfoPanel::paint(juce::Graphics& g)
{
    juce::Rectangle<int> area = getLocalBounds();
    
    g.fillAll (juce::Colours::black);

    g.setColour(juce::Colours::white);
    g.drawRect(area, 4);
    
    area = area.reduced(4);
    
    juce::Rectangle<int> header = area.removeFromTop(InfoPanelHeaderHeight);
    g.setColour(juce::Colours::blue);
    g.fillRect(header);
    juce::Font font (InfoPanelHeaderHeight * 0.8f);
    g.setFont(font);
    g.setColour(juce::Colours::white);
    if (midi)
      g.drawText("MIDI Bindings", header, juce::Justification::centred);
    else
      g.drawText("Key Bindings", header, juce::Justification::centred);
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

const int InfoPanelTriggerColumn = 1;
const int InfoPanelTargetColumn = 2;
const int InfoPanelScopeColumn = 3;
const int InfoPanelArgumentsColumn = 4;
const int InfoPanelSourceColumn = 5;
        
void InfoPanel::initColumns()
{
    juce::TableHeaderComponent& header = table.getHeader();

    // columnId, width, minWidth, maxWidth, propertyFlags, insertIndex
    // minWidth defaults to 30
    // maxWidth to -1
    // propertyFlags=defaultFlags
    // insertIndex=-1
    // propertyFlags has various options for visibility, sorting, resizing, dragging
    // example used 1 based column ids, is that necessary?

    header.addColumn(juce::String("Trigger"), InfoPanelTriggerColumn,
                     100, 30, -1,
                     juce::TableHeaderComponent::defaultFlags);

    header.addColumn(juce::String("Target"), InfoPanelTargetColumn,
                     200, 30, -1,
                     juce::TableHeaderComponent::defaultFlags);

    header.addColumn(juce::String("Scope"), InfoPanelScopeColumn,
                     50, 30, -1,
                     juce::TableHeaderComponent::defaultFlags);

    header.addColumn(juce::String("Arguments"), InfoPanelArgumentsColumn,
                     50, 30, -1,
                     juce::TableHeaderComponent::defaultFlags);

    header.addColumn(juce::String("Source"), InfoPanelSourceColumn,
                     200, 30, -1,
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
    if (columnId == InfoPanelTriggerColumn) {
        if (midi) {
            cell = renderMidiTrigger(b);
        }
        else {
            // not currently storing modifiers in the Binding
            cell = KeyTracker::getKeyText(b->triggerValue, 0);
        }
    }
    else if (columnId == InfoPanelTargetColumn) {
        cell = juce::String(b->getSymbolName());
    }
    else if (columnId == InfoPanelScopeColumn) {
        cell = juce::String(b->getScope());
    }
    else if (columnId == InfoPanelArgumentsColumn) {
        cell = juce::String(b->getArguments());
    }
    else if (columnId == InfoPanelSourceColumn) {
        cell = juce::String(b->getSource());
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

//////////////////////////////////////////////////////////////////////
// Drag
//////////////////////////////////////////////////////////////////////

// Working pretty well, but you can drag it completely out of the
// containing window.  Need to prevent dragging when it reaches some
// threshold.  If that isn't possible, let it finish, then snap it back to
// ensure at least part of it is visible.


void InfoPanel::mouseDown(const juce::MouseEvent& e)
{
    // ugh, this panel doesn't have a header yet
    // give it a drag sensitivity region but that's going to
    // overlap with the sorting table headers
    //if (e.getMouseDownY() < TestPanelHeaderHeight) {
    if (e.getMouseDownY() < 20) {
        dragger.startDraggingComponent(this, e);

        // the first argu is "minimumWhenOffTheTop" set
        // this to the full height and it won't allow dragging the
        // top out of boundsa
        dragConstrainer.setMinimumOnscreenAmounts(getHeight(), 100, 100, 100);
        
        dragging = true;
    }
}

void InfoPanel::mouseDrag(const juce::MouseEvent& e)
{
    // dragger.dragComponent(this, e, nullptr);
    dragger.dragComponent(this, e, &dragConstrainer);
    
    if (!dragging)
      Trace(1, "InfoPanel: mosueDrag didn't think it was dragging\n");
}

// don't need any of this logic, left over from when I was learning
void InfoPanel::mouseUp(const juce::MouseEvent& e)
{
    if (dragging) {
        if (e.getDistanceFromDragStartX() != 0 ||
            e.getDistanceFromDragStartY() != 0) {

            // is this the same, probably not sensitive to which button
            if (!e.mouseWasDraggedSinceMouseDown()) {
                Trace(1, "InfoPanel: Juce didn't think it was dragging\n");
            }
            
            //Trace(2, "InfoPanel: New location %d %d\n", getX(), getY());
            
            //area->saveLocation(this);
            dragging = false;
        }
        else if (e.mouseWasDraggedSinceMouseDown()) {
            Trace(1, "InfoPanel: Juce thought we were dragging but the position didn't change\n");
        }
    }
    else if (e.mouseWasDraggedSinceMouseDown()) {
        Trace(1, "InfoPanel: Juce thought we were dragging\n");
    }

    dragging = false;
}
