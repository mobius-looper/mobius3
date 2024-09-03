/**
 * A table showing the files loaded into the script library.
 */

#pragma once

#include <JuceHeader.h>

#include "../common/ButtonBar.h"
#include "../../script/ScriptRegistry.h"
#include "../../script/MslScriptUnit.h"

#include "ScriptFileDetails.h"

/**
 * Represents one file in the library.
 */
class ScriptLibraryTableFile
{
  public:
    ScriptLibraryTableFile() {
    }
    ScriptLibraryTableFile(ScriptRegistry::File* argFile) {
        file = argFile;
    }
    ~ScriptLibraryTableFile() {
    }

    ScriptRegistry::File* file = nullptr;

    bool hasErrors() {
        return file->hasErrors();
    }
    
};

class ScriptLibraryTable : public juce::Component,
                           public juce::TableListBoxModel,
                           public ButtonBar::Listener
{
  public:

    const int ColumnName = 1;
    const int ColumnStatus = 2;
    const int ColumnPath = 3;
    
    ScriptLibraryTable(class Supervisor* s);
    ~ScriptLibraryTable();

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
    void selectedRowsChanged(int lastRowSelected);

  private:
    class Supervisor* supervisor = nullptr;

    juce::OwnedArray<class ScriptLibraryTableFile> files;
    
    ButtonBar commands;
    juce::TableListBox table { {} /* component name */, this /* TableListBoxModel */};

    ScriptFileDetails details;
    

    void initTable();
    void initColumns();
    juce::String getCellText(int rowNumber, int columnId);
};
    
