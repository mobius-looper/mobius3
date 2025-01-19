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
    int number = 0;
    bool midi = false;

};

class SessionTrackTable : public TypicalTable
{
  public:

    const int ColumnName = 1;
    
    SessionTrackTable();
    ~SessionTrackTable();

    void initialize(class Provider* p);
    void load(class Provider* p, class Session* s);
    
    int getSelectedTrackNumber();
    int getTrackNumber(int row);

    bool isMidi(int row);

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
    
