/**
 * A table showing the exported symbols (functions and variables)
 * accessible from the library and external files.
 *
 * This is basically the "Linkages" table which is the most useful for seeing
 * what the scripts are providing.  The ScriptLibraryTable shows the files in the
 * library which are often callable symbols but each file can have multiple
 * symbols and library files won't have any for the file itself.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "../common/ButtonBar.h"
#include "../../script/ScriptRegistry.h"
#include "../../script/MslDetails.h"

/**
 * Represents one file in the library.
 */
class ScriptSymbolTableRow
{
  public:
    ScriptSymbolTableRow() {
    }
    ~ScriptSymbolTableRow() {
    }

    class Symbol* symbol = nullptr;
    juce::String location;
    class ScriptRegistry::File* registryFile = nullptr;
    
};

class ScriptSymbolTable : public juce::Component,
                          public juce::TableListBoxModel,
                          public ButtonBar::Listener
{
  public:

    const int ColumnName = 1;
    const int ColumnType = 2;
    const int ColumnLocation = 3;
    
    ScriptSymbolTable(class Supervisor* s);
    ~ScriptSymbolTable();

    /**
     * Build the table from a ScriptConfig
     * Ownership is not taken.
     */
    void load(class ScriptRegistry* reg);
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
    //void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void selectedRowsChanged(int lastRowSelected) override;

  private:
    class Supervisor* supervisor = nullptr;

    juce::OwnedArray<class ScriptSymbolTableRow> symbols;
    
    ButtonBar commands;
    juce::TableListBox table { {} /* component name */, this /* TableListBoxModel */};

    void initTable();
    void initColumns();
    juce::String getCellText(int rowNumber, int columnId);
};
    
