/**
 * A table showing finished script results.
 */

#pragma once

#include <JuceHeader.h>

#include "TypicalTable.h"

class ScriptProcessTableRow
{
  public:
    ScriptProcessTableRow() {
    }
    ~ScriptProcessTableRow() {
    }

    juce::String name;
    juce::String status;
    juce::String session;
    
};

class ScriptProcessTable : public TypicalTable
{
  public:

    const int ColumnName = 1;
    const int ColumnStatus = 2;
    const int ColumnSession = 3;
    
    ScriptProcessTable(class Supervisor* s);
    ~ScriptProcessTable();

    void load();
    void clear();

    // TypicalTable overrides
    int getRowCount() override;
    juce::String getCellText(int rowNumber, int columnId) override;
    void doCommand(juce::String name) override;

  private:
    
    class Supervisor* supervisor = nullptr;
    juce::OwnedArray<class ScriptProcessTableRow> processes;
    
};
    
