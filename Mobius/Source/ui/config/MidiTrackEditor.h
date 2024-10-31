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
    void initInputDevice(Session::Track* track);
    void initOutputDevice(Session::Track* track);

    void saveSession();
    void saveTrack(int index);

    std::unique_ptr<class Session> session;
    std::unique_ptr<class Session> revertSession;

    int selectedTrack = 0;
    BasicTabs tabs;


    YanForm rootForm;
    YanInput trackCount {"Active Tracks"};
    YanRadio trackSelector {"Track"};

    YanParameterForm generalForm;
    YanCombo inputDevice {"Input Device"};
    YanCombo outputDevice {"Output Device"};
    YanCheckbox midiThru {"MIDI Thru"};

    YanParameterForm switchForm;

    YanParameterForm followerForm;
    YanCombo leader {"Leader Track"};;
    YanCheckbox followRecord {"Follow Record"};
    YanCheckbox followMute {"Follow Mute"};
    YanCheckbox followSize {"Follow Size"};
    YanCheckbox followLocation {"Follow Location"};
    
};
