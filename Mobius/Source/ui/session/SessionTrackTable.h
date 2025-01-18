/**
 * A table showing configured track summaries
 */

#pragma once

#include <JuceHeader.h>

#include "../script/TypicalTable.h"

class SessionTrackTableRow
{
  public:
    SessionTrackTableRow() {
    }
    ~SessionTrackTableRow() {
    }

    juce::String name;

};

class SessionTrackTable : public TypicalTable
{
  public:

    const int ColumnName = 1;
    
    SessionTrackTable(class Provider* p);
    ~SessionTrackTable();

    void load();
    void clear();

    // TypicalTable overrides
    int getRowCount() override;
    juce::String getCellText(int rowNumber, int columnId) override;
    void doCommand(juce::String name) override;

  private:
    
    class Provider* provider = nullptr;
    juce::OwnedArray<class SessionTrackTableRow> tracks;
    
};
    
