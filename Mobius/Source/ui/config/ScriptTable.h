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
class ScriptTableFile
{
  public:
    ScriptTableFile() {
    }
    ScriptTableFile(juce::String argPath) {
        path = argPath;
    }
    ~ScriptTableFile() {
    }

    juce::String path;
    // anything else of intereset in here, maybe size, format or something
    // Sample has a bunch of operational flags

    // set true if we determine this file does not exist
    bool missing = false;
};

class ScriptTable : public juce::Component, public juce::TableListBoxModel, public ButtonBar::Listener
{
  public:
    
    ScriptTable(class Supervisor* s);
    ~ScriptTable();

    /**
     * Build the table from a ScriptConfig
     * Ownership is not taken.
     */
    void setScripts(class ScriptConfig* scripts);
    void updateContent();
    void clear();
    ScriptConfig* capture();

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

    juce::OwnedArray<class ScriptTableFile> files;
    
    ButtonBar commands;
    juce::TableListBox table { {} /* component name */, this /* TableListBoxModel */};

    int fileColumn = 0;

    void initTable();
    void initColumns();
    juce::String getCellText(int rowNumber, int columnId);

    void doFileChooser();
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastFolder;

};
    
