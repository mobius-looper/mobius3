/**
 * Display all of the interned symbols for debugging.
 */

#include <JuceHeader.h>

#include "../model/Symbol.h"
#include "../model/FunctionProperties.h"

#include "../ui/common/BasicTable.h"
#include "../ui/BasePanel.h"

#include "../Supervisor.h"

#include "SymbolTablePanel.h"

const int SymbolTableNameColumn = 1;
const int SymbolTableTypeColumn = 2;
const int SymbolTableLevelColumn = 3;
const int SymbolTableFlagsColumn = 4;
const int SymbolTableWarnColumn = 5;

SymbolTableContent::SymbolTableContent(Supervisor* s)
{
    supervisor = s;
    table.setBasicModel(this);
    table.addColumn("Symbol", SymbolTableNameColumn, 150);
    table.addColumn("Type", SymbolTableTypeColumn, 100);
    table.addColumn("Level", SymbolTableLevelColumn, 100);
    table.addColumn("Flags", SymbolTableFlagsColumn, 100);
    table.addColumn("Warnings", SymbolTableWarnColumn, 100);
    addAndMakeVisible(table);
}

SymbolTableContent::~SymbolTableContent()
{
}

void SymbolTableContent::prepare()
{
    symbols.clear();
    for (auto symbol : supervisor->getSymbols()->getSymbols()) {
        symbols.add(symbol);
    }
    table.updateContent();
}

void SymbolTableContent::resized()
{
    table.setBounds(getLocalBounds());
}

int SymbolTableContent::getNumRows()
{
    return symbols.size();
}

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
        else if (s->functionProperties != nullptr) {
            cell = "Function";
        }
        else if (s->parameterProperties != nullptr) {
            cell = "Parameter";
        }
        else if (s->script != nullptr) {
            cell = "Script";
        }
        else if (s->sample != nullptr) {
            cell = "Sample";
        }
        else if (s->coreFunction != nullptr) {
            // internal function without FunctionProperties
            cell = "Core Function";
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
            case LevelNone: cell = "None"; break;
            case LevelUI: cell = "UI"; break;
            case LevelShell: cell = "Shell"; break;
            case LevelKernel: cell = "Kernel"; break;
            case LevelTrack: cell = "Track"; break;
        }
    }
    else if (columnId == SymbolTableFlagsColumn) {
        // expose interesting things so we know to put them in FunctionProperties
        if (s->functionProperties != nullptr) {
            if (s->functionProperties->sustainable) cell = "sustainable ";
            if (s->functionProperties->mayFocus) cell += "focus ";
            if (s->functionProperties->mayConfirm) cell += "confirm ";
            if (s->functionProperties->mayCancelMute) cell += "muteCancel ";
        }
    }
    else if (columnId == SymbolTableWarnColumn) {
        if (s->coreFunction != nullptr && s->functionProperties == nullptr) {
            // core function not exposed in bindings
            // !! I don't think this works any more now that Mobius bootstraps
            // FunctionProperties, should leave a flag instead
            cell = "Core function not exposed";
        }
        else if (s->functionProperties != nullptr && s->level == LevelTrack && s->coreFunction == nullptr) {
            // !! this isn't accurate, LevelTrack just means it is at the lowest level
            // the track implementation doesn't necessarily need a coreFunction, only
            // Mobius audio tracks do
            cell = "Core function not implemented";
        }
        
    }
    
    return cell;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
