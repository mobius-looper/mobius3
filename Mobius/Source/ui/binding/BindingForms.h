
#pragma once

#include <JuceHeader.h>

#include "../common/YanField.h"
#include "../common/YanForm.h"

class BindingForms : public juce::Component, YanCombo::Listener, YanInput::Listener
{
  public:

    BindingForms();

    void load(class Provider* p, class Binding* b);
    
    void resized() override;

    // YanField Listeners
    void yanInputChanged(class YanInput* i) override;
    void yanComboSelected(class YanCombo* c, int selection) override;
    
  private:

    class Provider* provider = nullptr;
    int maxTracks = 0;
    
    bool showKey = false;
    bool showMidi = false;
    
    juce::Label title;
    juce::Label triggerTitle;
    juce::Label targetTitle;

    //YanForm commonForm;
    //YanCombo triggerType {"Trigger Type"};
    //YanCheckbox activeBox {"Active"};;

    YanForm midiForm;
    YanCombo midiType {"Type"};
    YanCombo midiChannel {"Channel"};
    YanInput midiValue {"Value"};
    YanCheckbox midiRelease {"Release"};
    YanCheckbox midiCapture {"Capture"};
    YanInput midiSummary {""};
    
    YanForm keyForm;
    YanInput keyValue {"Key"};
    YanCheckbox keyRelease {"Release"};
    YanCheckbox keyCapture {"Capture"};
    YanInput keySummary {""};
    
    YanForm qualifiers;
    YanCombo scope {"Scope"};
    YanInput arguments {"Arguments"};
    
    void refreshScopeNames();
    void refreshScopeValue(class Binding* b);
};
