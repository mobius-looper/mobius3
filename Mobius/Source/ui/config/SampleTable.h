/**
 * A table showing a list of Samples within a SampleConfig.
 *
 * This is almost identical to ScriptTable, share something
 *
 */

#pragma once

#include <JuceHeader.h>

#include "../common/ButtonBar.h"

/**
 * Represents one sample file in the table.
 */
class SampleFile
{
  public:
    SampleFile() {
    }
    SampleFile(juce::String argPath) {
        path = argPath;
    }
    ~SampleFile() {
    }

    juce::String path;
    // anything else of intereset in here, maybe size, format or something
    // Sample has a bunch of operational flags
    bool missing = false;
};

class SampleTable : public juce::Component, public juce::TableListBoxModel, public ButtonBar::Listener
{
  public:
    
    SampleTable(class Supervisor* s);
    ~SampleTable();

    /**
     * Build the table from a SampleConfig
     * Ownership is not taken.
     */
    void setSamples(class SampleConfig* samples);
    void updateContent();
    void clear();
    SampleConfig* capture();

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

    juce::OwnedArray<class SampleFile> files;
    
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
    
