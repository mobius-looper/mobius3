/**
 * ConfigEditor for editing the Setups.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/Setup.h"
#include "../common/Form.h"
#include "../common/SimpleRadio.h"
#include "../common/SimpleButton.h"

#include "ConfigEditor.h"

class SetupEditor : public ConfigEditor,
                    public SimpleRadio::Listener,
                    public juce::Button::Listener,
                    public juce::ComboBox::Listener
{
  public:
    
    SetupEditor(class Supervisor* s);
    ~SetupEditor();

    juce::String getTitle() override {return "Setups";}

    void prepare() override;
    void load() override;
    void save() override;
    void cancel() override;
    void revert() override;
    
    void objectSelectorSelect(int ordinal) override;
    void objectSelectorRename(juce::String newName) override;
    void objectSelectorDelete() override;
    void objectSelectorNew(juce::String name) override;

    void resized() override;

    void radioSelected(class SimpleRadio* r, int index) override;
    void buttonClicked(juce::Button* b) override;
    void comboBoxChanged(juce::ComboBox* combo) override;
    
  private:
    
    void refreshObjectSelector();
    void adjustTrackSelector();
    void render();
    void initForm();
    void addField(const char* tab, class UIParameter* p);
    class Field* buildResetablesField();
    
    void loadSetup(int index);
    void saveSetup(int index);
    Setup* getSelectedSetup();
    
    juce::OwnedArray<Setup> setups;
    juce::OwnedArray<Setup> revertSetups;
    // this will be in FormPanel's OwnedArray
    SimpleRadio* trackSelector = nullptr;
    juce::ComboBox* trackCombo = nullptr;
    SimpleButton* initButton = nullptr;
    SimpleButton* initAllButton = nullptr;
    SimpleButton* captureButton = nullptr;
    SimpleButton* captureAllButton = nullptr;
    int selectedSetup = 0;
    int selectedTrack = 0;
    int trackCount = 0;
    
    Form form;

};
