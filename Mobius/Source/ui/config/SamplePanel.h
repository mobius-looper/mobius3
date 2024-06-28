/**
 * ConfigPanel to edit scripts
 */

#pragma once

#include <JuceHeader.h>

#include "NewConfigPanel.h"
#include "SampleTable.h"

class SampleEditor : public ConfigPanelContent
{
  public:
    SampleEditor();
    ~SampleEditor();

    // overloads called by ConfigPanel
    void load() override;
    void save() override;
    void cancel() override;

    void resized() override;

  private:

    SampleTable table;
    juce::String lastFolder;

};

class SamplePanel : public NewConfigPanel
{
  public:
    SamplePanel() {
        setName("SamplePanel");
        setTitle("Samples");
        setConfigContent(&editor);
    }
    ~SamplePanel() {
    }

  private:
    SampleEditor editor;
};
