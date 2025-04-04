
#pragma once

#include <JuceHeader.h>

#include "../common/YanField.h"
#include "../common/YanForm.h"

class BindingForms : public juce::Component, YanCombo::Listener, YanInput::Listener
{
  public:

    /**
     * The trigger form configures itself for one of these types.
     */
    typedef enum {
        TypeUnknown,
        TypeMidi,
        TypeKey,
        TypeHost
    } Type;

    BindingForms();

    void load(class Provider* p, class Binding* b);
    void save(Binding* b);
    
    void resized() override;

    // YanField Listeners
    void yanInputChanged(class YanInput* i) override;
    void yanComboSelected(class YanCombo* c, int selection) override;
    
  private:

    class Provider* provider = nullptr;
    Type type = TypeUnknown;
    int maxTracks = 0;
    int capturedCode = 0;
    
    juce::Label title;
    juce::Label triggerTitle;
    juce::Label targetTitle;

    // This was "passthrough" which I think caused any MIDI being
    // received to be treated as if it was an active binding and
    // sent to the engine, I think jmage wanted this for testing
    // unclear if that's a good idea
    //YanCheckbox activeBox {"Active"};

    YanForm triggerForm;

    YanCombo midiType {"Type"};
    YanCombo midiChannel {"Channel"};

    YanInput triggerValue {"Value"};
    YanCheckbox release {"Release"};
    
    YanCheckbox capture {"Capture"};
    YanInput captureText {""};
    
    YanForm qualifiers;
    YanCombo scope {"Scope"};
    YanInput arguments {"Arguments"};
    
    void refreshScopeNames();
    void refreshScopeValue(class Binding* b);
    int unpackKeyCode();
    juce::String unpackScope();
    
};
