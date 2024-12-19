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

    // the path guides the table, this is taken
    // directly from the ScriptRegistry::Machine::Externals list
    juce::String path;

    // the file type: MOS, MSL, Folder, Missing
    juce::String type;
    
    // set true if we determine this file does not exist
    // this is taken from the External path, if there is a FIle
    // it should match the missing flag there, but that may be stale
    // until the next ScriptClerk refresh
    bool missing = false;

    // if ScriptClerk has had the opportunity to process this file
    // it will create one of these with extra information, this is usually
    // there but not during periods of new file addition
    ScriptRegistry::File* registryFile = nullptr;
};

class ScriptExternalTable : public juce::Component, public juce::TableListBoxModel, public ButtonBar::Listener
{
  public:
    
    const int ColumnPath = 1;
    const int ColumnType = 2;
    const int ColumnStatus = 3;
    
    ScriptExternalTable(class Supervisor* s, class ScriptConfigEditor* parent);
    ~ScriptExternalTable();

    void load(class ScriptRegistry* reg);
    void updateContent();
    void clear();
    juce::StringArray getPaths();

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
    
