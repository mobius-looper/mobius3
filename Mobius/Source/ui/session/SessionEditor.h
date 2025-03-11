/**
 * ConfigEditor for editing the MIDI tracks.
 * This is actually a Session editor, and should expand this to include
 * other things in the session.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/Symbol.h"
#include "../common/BasicTabs.h"
#include "../config/ConfigEditor.h"
#include "SessionOcclusions.h"

class SessionEditor : public ConfigEditor, public BasicTabs::Listener
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

    void overlayChanged();
    void basicTabsChanged(int oldIndex, int newIndex) override;

    // utilities used by SessionTrackForms
    void gatherOcclusions(SessionOcclusions& occlusions, class ValueSet* values, SymbolId sid);
    
    SessionOcclusions::Occlusion* getOcclusion(class Symbol* s, SessionOcclusions& trackOcclusions);
    
  private:

    void loadSession();
    void saveSession(class Session* master);
    void invalidateSession();
    int getPortValue(class ValueSet* set, const char* name, int max);
    void refreshLocalOcclusions();

    std::unique_ptr<class Session> session;
    std::unique_ptr<class Session> revertSession;
    SessionOcclusions sessionOcclusions;
    SessionOcclusions defaultTrackOcclusions;

    BasicTabs tabs;
    
    std::unique_ptr<class SessionGlobalEditor> globalEditor;
    std::unique_ptr<class SessionParameterEditor> parameterEditor;
    std::unique_ptr<class SessionTrackEditor> trackEditor;
    
};
