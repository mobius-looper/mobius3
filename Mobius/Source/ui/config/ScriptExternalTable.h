/**
 * A table showing a list of Scripts within a ScriptConfig.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "../common/ButtonBar.h"

/**
 * Represents one sample file in the table.
 * todo: see if this can be replaced with the new script/ScriptFile
 */
class ScriptExternalTableFile
{
  public:
    ScriptExternalTableFile() {
    }
    ~ScriptExternalTableFile() {
    }

    ScriptRegistry::File* file = nullptr;
    juce::String filename;
    juce::String refname;
    juce::String folder;
    juce::String path;
    // anything else of intereset in here, maybe size, format or something
    // Sample has a bunch of operational flags

    // set true if we determine this file does not exist
    bool missing = false;
};

class ScriptExternalTable : public juce::Component, public juce::TableListBoxModel, public ButtonBar::Listener
{
  public:
    
    const int ColumnName = 1;
    const int ColumnRefname = 2;
    const int ColumnNamespace = 3;
    const int ColumnStatus = 4;
    const int ColumnFolder = 5;
    
    ScriptExternalTable(class Supervisor* s, class ScriptConfigEditor* parent);
    ~ScriptExternalTable();

    void load(class ScriptRegistry* reg);
    void updateContent();
    void clear();
    juce::StringArray getResult();

    int getPreferredWidth();
    int getPreferredHeight();

    // ButtonBar::Listener
    void buttonClicked(juce::String name) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;

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
    class ScriptConfigEditor* parent = nullptr;
    
    juce::OwnedArray<class ScriptExternalTableFile> files;
    
    ButtonBar commands;
    juce::TableListBox table { {} /* component name */, this /* TableListBoxModel */};

    void initTable();
    void initColumns();
    juce::String getCellText(int rowNumber, int columnId);

    void doFileChooser();
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastFolder;

};
    
