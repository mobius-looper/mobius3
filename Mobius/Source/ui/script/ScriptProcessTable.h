/**
 * A table showing finished script results.
 */

#pragma once

#include <JuceHeader.h>

#include "../common/ButtonBar.h"

class ScriptProcessTableRow
{
  public:
    ScriptProcessTableRow() {
    }
    ~ScriptProcessTableRow() {
    }

    //MslProcess* result = nullptr;
};

class ScriptProcessTable : public juce::Component,
                          public juce::TableListBoxModel,
                          public ButtonBar::Listener
{
  public:

    const int ColumnName = 1;
    const int ColumnType = 2;
    const int ColumnLocation = 3;
    
    ScriptProcessTable(class Supervisor* s);
    ~ScriptProcessTable();

    void load();
    void updateContent();
    void clear();

    int getPreferredWidth();
    int getPreferredHeight();

    // ButtonBar::Listener
    void buttonClicked(juce::String name) override;

    // Component
    void resized() override;

    // TableListBoxModel
    
    int getNumRows() override;
    void paintRowBackground (juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected) override;
    void paintCell (juce::Graphics& g, int rowNumber, int columnId,
                    int width, int height, bool rowIsSelected) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void selectedRowsChanged(int lastRowSelected) override;

  private:
    class Supervisor* supervisor = nullptr;

    juce::OwnedArray<class ScriptProcessTableRow> processes;
    
    ButtonBar commands;
    juce::TableListBox table { {} /* component name */, this /* TableListBoxModel */};

    void initTable();
    void initColumns();
    juce::String getCellText(int rowNumber, int columnId);
};
    
