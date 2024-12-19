/**
 * A table showing finished script results.
 */

#pragma once

#include <JuceHeader.h>

#include "TypicalTable.h"

class ScriptResultTableRow
{
  public:
    ScriptResultTableRow() {
    }
    ~ScriptResultTableRow() {
    }

    juce::String name;
    juce::String session;
    juce::String value;
    juce::String error;
    
};

class ScriptResultTable : public TypicalTable
{
  public:

    const int ColumnName = 1;
    const int ColumnSession = 2;
    const int ColumnValue = 3;
    const int ColumnError = 4;
    
    ScriptResultTable(class Supervisor* s);
    ~ScriptResultTable();

    void load();
    void clear();

    // TypicalTable overrides
    int getRowCount() override;
    juce::String getCellText(int rowNumber, int columnId) override;
    void doCommand(juce::String name) override;

  private:
    
    class Supervisor* supervisor = nullptr;
    juce::OwnedArray<class ScriptResultTableRow> results;
    
};
    
