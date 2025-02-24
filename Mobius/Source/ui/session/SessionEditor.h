/**
 * ConfigEditor for editing the MIDI tracks.
 * This is actually a Session editor, and should expand this to include
 * other things in the session.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "../common/BasicTabs.h"
#include "../config/ConfigEditor.h"

class SessionEditor : public ConfigEditor
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
    void decacheForms() override;
    
    void resized() override;

    // because we build forms dynamically, inner components need
    // to get to the Session being edited
    class Session* getEditingSession();

    class Provider* getProvider();
    
  private:

    void loadSession();
    void saveSession(class Session* master);
    void invalidateSession();
    int getPortValue(class ValueSet* set, const char* name, int max);

    std::unique_ptr<class Session> session;
    std::unique_ptr<class Session> revertSession;

    BasicTabs tabs;
    
    std::unique_ptr<class SessionGlobalEditor> globalEditor;
    std::unique_ptr<class SessionParameterEditor> parameterEditor;
    std::unique_ptr<class SessionTrackEditor> trackEditor;
};
