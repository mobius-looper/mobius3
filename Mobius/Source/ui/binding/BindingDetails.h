
#pragma once

#include <JuceHeader.h>

#include "../BasePanel.h"
#include "../../MidiManager.h"
#include "../../KeyTracker.h"

#include "BindingTree.h"

class BindingDetailsListener {
  public:
    virtual ~BindingDetailsListener() {}
    virtual void bindingSaved() = 0;
    virtual void bindingCanceled() = 0;
};

class BindingContent : public juce::Component,
                       public MidiManager::Monitor,
                       public KeyTracker::Listener                       
{
  public:

            
    /**
     * The trigger form configures itself for one of these types.
     */
    typedef enum {
        TypeUnknown,
        TypeMidi,
        TypeKey,
        TypeHost,
        TypeButton
    } Type;

    static const int FontHeight = 20;
    static const int TextHeight = 100;

    BindingContent();
    ~BindingContent();

    void initialize(class Supervisor* s);
    void load(BindingDetailsListener* l, class Binding* b);
    void save();
    void cancel();
    
    bool isCapturing();
    void setCapturing(bool b);
    
    void resized() override;

    void midiMonitor(const juce::MidiMessage& message, juce::String& source) override;
    bool midiMonitorExclusive() override;
    
    void keyTrackerDown(int code, int modifiers) override;
    void keyTrackerUp(int code, int modifiers) override;
    
  private:

    class Supervisor* supervisor = nullptr;
    BindingDetailsListener* listener = nullptr;
    Binding* binding = nullptr;
    bool capturing = false;
    
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
    
    void trackKeys();
    void trackMidi();
    void closeTrackers();

    juce::String renderCapture(const juce::MidiMessage& msg);
    void showCapture(juce::String& cap);
    
    void save(class Binding* b);
};    

class BindingDetailsPanel : public BasePanel
{
  public:

    BindingDetailsPanel();
    ~BindingDetailsPanel() {}

    void initialize();
    void initialize(class Supervisor* s);

    void setCapturing(bool b) {
        content.setCapturing(b);
    }

    bool isCapturing() {
        return content.isCapturing();
    }

    void show(juce::Component* parent, BindingDetailsListener* l, class Binding* b);
    void close() override;
    
    // BasePanel button handler
    void footerButton(juce::Button* b) override;
    
  private:

    juce::TextButton saveButton {"Save"};
    juce::TextButton cancelButton {"Cancel"};
    BindingContent content;
};

