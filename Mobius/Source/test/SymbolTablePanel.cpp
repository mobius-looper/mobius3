/**
 * Display all of the interned symbols for debugging.
 */

#include <JuceHeader.h>

#include "../model/Symbol.h"
#include "../model/FunctionDefinition.h"

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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
