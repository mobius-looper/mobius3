/**
 * Display all of the interned symbols for debugging.
 */

#include <JuceHeader.h>

#include "../model/Symbol.h"
#include "../model/FunctionDefinition.h"

#include "../Supervisor.h"
#include "../ui/BasePanel.h"

#include "SymbolTablePanel.h"

SymbolTableContent::SymbolTableContent()
{
    initTable();
    addAndMakeVisible(table);
}

SymbolTableContent::~SymbolTableContent()
{
}

void SymbolTableContent::prepare()
{
    symbols.clear();
    for (auto symbol : Symbols.getSymbols()) {
        symbols.add(symbol);
    }
    table.updateContent();
}

void SymbolTableContent::resized()
{
    table.setBounds(getLocalBounds());
}

//////////////////////////////////////////////////////////////////////
//
// TableListBoxModel
//
//////////////////////////////////////////////////////////////////////

void SymbolTableContent::initTable()
{
    table.setColour (juce::ListBox::outlineColourId, juce::Colours::grey);
    table.setOutlineThickness (1);
    table.setMultipleSelectionEnabled (false);
    table.setClickingTogglesRowSelection(true);
    table.setHeaderHeight(22);
    table.setRowHeight(22);

    initColumns();

}

const int SymbolTableNameColumn = 1;
const int SymbolTableTypeColumn = 2;
const int SymbolTableLevelColumn = 3;
const int SymbolTableFlagsColumn = 4;
const int SymbolTableWarnColumn = 5;

void SymbolTableContent::initColumns()
{
    juce::TableHeaderComponent& header = table.getHeader();

    // columnId, width, minWidth, maxWidth, propertyFlags, insertIndex
    // minWidth defaults to 30
    // maxWidth to -1
    // propertyFlags=defaultFlags
    // insertIndex=-1
    // propertyFlags has various options for visibility, sorting, resizing, dragging
    // example used 1 based column ids, is that necessary?

    header.addColumn(juce::String("Symbol"), SymbolTableNameColumn,
                     150, 30, -1,
                     juce::TableHeaderComponent::defaultFlags);

    header.addColumn(juce::String("Type"), SymbolTableTypeColumn,
                     100, 30, -1,
                     juce::TableHeaderComponent::defaultFlags);
    
    header.addColumn(juce::String("Level"), SymbolTableLevelColumn,
                     100, 30, -1,
                     juce::TableHeaderComponent::defaultFlags);
    
    header.addColumn(juce::String("Flags"), SymbolTableFlagsColumn,
                     100, 30, -1,
                     juce::TableHeaderComponent::defaultFlags);
    
    header.addColumn(juce::String("Warnings"), SymbolTableWarnColumn,
                     100, 30, -1,
                     juce::TableHeaderComponent::defaultFlags);
}

/**
 * Callback from TableListBoxModel to derive the text to paint in this cell.
 * Row is zero based columnId is 1 based and is NOT a column index, you have
 * to map it to the logical column if allowing column reording in the table.
 */
juce::String SymbolTableContent::getCellText(int row, int columnId)
{
    juce::String cell;
    Symbol* s = symbols[row];
    
    if (columnId == SymbolTableNameColumn) {
        cell = s->getName();
        if (s->displayName.length() > 0)
          cell += "/" + s->displayName;
    }
    else if (columnId == SymbolTableTypeColumn) {
        if (s->variable != nullptr) {
            cell = "Variable";
        }
        else if (s->function != nullptr) {
            cell = "Function";
        }
        else if (s->parameter != nullptr) {
            cell = "Parameter";
        }
        else if (s->structure != nullptr) {
            cell = "Structure";
        }
        else if (s->script != nullptr) {
            cell = "Script";
        }
        else if (s->sample != nullptr) {
            cell = "Sample";
        }
        else if (s->coreFunction != nullptr) {
            // internal function without FunctionDefinition
            cell = "Core Function";
        }
        else if (s->coreParameter != nullptr) {
            cell = "Core Parameter";
        }
        else {
            switch (s->behavior) {
                // intrinsic functions/parameters
                case BehaviorParameter: cell = "Parameter: " + juce::String(s->id); break;
                case BehaviorFunction: cell = "Function: " + juce::String(s->id); break;
                // Setup/Preset activation
                case BehaviorActivation: cell = "Activation"; break;
                // shouldn't see these without a property attachment
                case BehaviorScript: cell = "Script ?"; break;
                case BehaviorSample: cell = "Sample ?"; break;
                default: cell = "???"; break;
            }
        }
    }
    else if (columnId == SymbolTableLevelColumn) {
        switch (s->level) {
            case LevelNone: cell = "Custom"; break;
            case LevelUI: cell = "UI"; break;
            case LevelShell: cell = "Shell"; break;
            case LevelKernel: cell = "Kernel"; break;
            case LevelCore: cell = "Core"; break;
        }
    }
    else if (columnId == SymbolTableFlagsColumn) {
        // expose interesting things
        if (s->function != nullptr && s->function->sustainable)
          cell = "sustainable";
    }
    else if (columnId == SymbolTableWarnColumn) {
        if (s->coreFunction != nullptr && s->function == nullptr) {
            // core function not exposed in bindings
            cell = "Core function not exposed";
        }
        else if (s->function != nullptr && s->level == LevelCore && s->coreFunction == nullptr) {
            cell = "Core function not implemented";
        }
        else if (s->coreParameter != nullptr && s->parameter == nullptr) {
            cell = "Core parameter not exposed";
        }
        else if (s->parameter != nullptr && s->level == LevelCore && s->coreParameter == nullptr) {
            cell = "Core parameter not implemented";
        }
        else if (s->function != nullptr && s->functionProperties == nullptr) {
            cell = "Function without FunctionProperties";
        }
        else if (s->function == nullptr && s->functionProperties != nullptr) {
            cell = "FunctionProperties without function definition";
        }
        else if (s->parameter != nullptr && s->parameterProperties == nullptr) {
            cell = "UIParameter without ParameterProperties";
        }
        else if (s->parameter == nullptr && s->parameterProperties != nullptr) {
            cell = "ParameterProperties without UIParameter";
        }
        
    }
    
    return cell;
}

/**
 * The maximum of all column rows.
 * This is independent of the table size.
 */
int SymbolTableContent::getNumRows()
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
 *
 * todo: I've used this in a bunch of places by now, factor out something
 * that can be shared
 */
void SymbolTableContent::paintRowBackground(juce::Graphics& g, int rowNumber,
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
 *
 * todo: I've carried around this analysis over multiple classes, get rid of it
 */
void SymbolTableContent::paintCell(juce::Graphics& g, int rowNumber, int columnId,
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
void SymbolTableContent::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)rowNumber;
    (void)columnId;
    (void)event;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
