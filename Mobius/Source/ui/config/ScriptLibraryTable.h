/**
 * A table showing the files loaded into the script library.
 */

#pragma once

#include <JuceHeader.h>

#include "../common/ButtonBar.h"
#include "../../script/ScriptRegistry.h"

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
};

class ScriptLibraryTable : public juce::Component,
                           public juce::TableListBoxModel,
                           public ButtonBar::Listener
{
  public:
    
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
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;

  private:
    class Supervisor* supervisor = nullptr;

    juce::OwnedArray<class ScriptLibraryTableFile> files;
    
    ButtonBar commands;
    juce::TableListBox table { {} /* component name */, this /* TableListBoxModel */};

    int fileColumn = 0;

    void initTable();
    void initColumns();
    juce::String getCellText(int rowNumber, int columnId);
};
    
