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

#include "ParameterCategoryTree.h"
#include "SessionEditorForm.h"

#include "../config/ConfigEditor.h"

class SessionParameterEditor : public juce::Component
{
  public:
    
    SessionParameterEditor(class SessionEditor* ed);
    ~SessionParameterEditor() {}

    void resized() override;
    void paint(juce::Graphics& g) override;

    void show(juce::String category, juce::Array<class Symbol*>& symbols);
    void load();
    void save(class Session* dest);
    
  private:

    class SessionEditor* sessionEditor = nullptr;

    juce::OwnedArray<class SessionEditorForm> forms;
    juce::HashMap<juce::String,class SessionEditorForm*> formTable;
    class SessionEditorForm* currentForm = nullptr;
    
};


class SessionEditorParametersTab : public juce::Component, public SymbolTree::Listener
{
  public:

    SessionEditorParametersTab(class SessionEditor* ed);
    ~SessionEditorParametersTab() {}

    void loadSymbols(class Provider* p);
    void symbolTreeClicked(SymbolTreeItem* item) override;
    void load();
    void save(class Session* dest);
    
    void resized() override;
    
  private:
    
    ParameterCategoryTree tree;
    SessionParameterEditor peditor;

};

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

    void resized() override;

    // because we build forms dynamically, inner components need
    // to get to the Session being edited
    class Session* getEditingSession();

    class Provider* getProvider();
    
  private:

    void loadSession();
    void saveSession(class Session* master);

    std::unique_ptr<class Session> session;
    std::unique_ptr<class Session> revertSession;

    BasicTabs tabs;
    SessionEditorParametersTab petab {this};

    std::unique_ptr<class SessionTrackEditor> trackEditor;
    
};
