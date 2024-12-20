/**
 * A table showing finished script results.
 */

#pragma once

#include <JuceHeader.h>

#include "TypicalTable.h"

class ScriptStatisticsTableRow
{
  public:
    ScriptStatisticsTableRow() {
    }
    ~ScriptStatisticsTableRow() {
    }

    juce::String name;
    int runs = 0;
    int errors = 0;
    
};

class ScriptStatisticsTable : public TypicalTable
{
  public:

    const int ColumnName = 1;
    const int ColumnRuns = 2;
    const int ColumnErrors = 3;
    
    ScriptStatisticsTable(class Supervisor* s);
    ~ScriptStatisticsTable();

    void load();
    void clear();

    // TypicalTable overrides
    int getRowCount() override;
    juce::String getCellText(int rowNumber, int columnId) override;
    void doCommand(juce::String name) override;

  private:
    
    class Supervisor* supervisor = nullptr;
    juce::OwnedArray<class ScriptStatisticsTableRow> stats;
    
};
    
