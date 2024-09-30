/**
 * ConfigEditor for editing the MIDI tracks.
 * This is actually a Session editor, and should expand this to include
 * other things in the session.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/Session.h"
#include "../common/YanForm.h"
#include "../common/YanField.h"
#include "YanParameterForm.h"

#include "ConfigEditor.h"

class MidiTrackEditor : public ConfigEditor,
                        public YanRadio::Listener,
                        public YanCombo::Listener,
                        public YanInput::Listener
{
  public:
    
    MidiTrackEditor(class Supervisor* s);
    ~MidiTrackEditor();

    juce::String getTitle() override {return "MIDI Tracks";}

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
    void loadTrack(int index);
    void saveSession();
    void saveTrack(int index);

    std::unique_ptr<class Session> session;
    std::unique_ptr<class Session> revertSession;

    YanParameterForm form;
    YanInput trackCount {"Active Tracks"};
    YanRadio trackSelector {"Tracks"};
    int selectedTrack = 0;
    
};