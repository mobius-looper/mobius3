/**
 * A table showing available sessions.
 */

#pragma once

#include <JuceHeader.h>

#include "../script/TypicalTable.h"

class SessionManagerTableRow
{
  public:
    SessionManagerTableRow() {
    }
    ~SessionManagerTableRow() {
    }

    juce::String name;
    
};

class SessionManagerTable : public TypicalTable
{
  public:

    const int ColumnName = 1;
    
    SessionManagerTable(class Supervisor* s);
    ~SessionManagerTable();

    void load();
    void clear();

    // TypicalTable overrides
    int getRowCount() override;
    juce::String getCellText(int rowNumber, int columnId) override;
    void doCommand(juce::String name) override;

  private:
    
    class Supervisor* supervisor = nullptr;
    juce::OwnedArray<class SessionManagerTableRow> sessions;
    
};
    
