/**
 * ConfigEditor for editing the MIDI tracks.
 * This is actually a Session editor, and should expand this to include
 * other things in the session.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/Session.h"
#include "../common/BasicTabs.h"
#include "../common/YanForm.h"
#include "../common/YanField.h"

#include "SymbolTree.h"

#include "ConfigEditor.h"

class SessionEditor : public ConfigEditor,
                        public YanRadio::Listener,
                        public YanCombo::Listener,
                        public YanInput::Listener
{
  public:
    
    SessionEditor(class Supervisor* s);
    ~SessionEditor();

    juce::String getTitle() override {return "Session";}

    void prepare() override;
    void load() override;
    void save() override;
    void cancel() override;
    void revert() override;

    void resized() override;

    void radioSelected(class YanRadio* r, int index) override;
    void comboSelected(class YanCombo* c, int index) override;
    void inputChanged(class YanInput* i) override;
    
  private:

    void render();
    void initForm();
    
    void loadSession();
    void saveSession(class Session* master);

    std::unique_ptr<class Session> session;
    std::unique_ptr<class Session> revertSession;

    BasicTabs tabs;
    SymbolTree tree;

    YanForm transportForm;
    YanCheckbox midiOut {"MIDI Out"};
    YanCheckbox midiClocks {"MIDI Clocks When Stopped"};
    
};
