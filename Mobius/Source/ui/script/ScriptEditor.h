/**
 * This is effectively the root of the script editor UI, though it is contained
 * under two layers:
 *
 *     ScriptWindow which is a DocumentWindow
 *     ScriptWindowContent which is the single main component for the DocumentWindow
 *
 * The Editor will maintain a set of tabs for each script file loaded into it.
 *
 */

#pragma once

#include "../common/BasicTabs.h"
#include "../common/BasicButtonRow.h"
#include "../../script/ScriptRegistry.h"
#include "ScriptDetails.h"
#include "CustomEditor.h"

/**
 * Represents one file loaded into the editor.
 */
class ScriptEditorFile : public juce::Component
{
  public:

    ScriptEditorFile(class ScriptEditor* e, class ScriptRegistry::File* src);
    ~ScriptEditorFile();

    void refresh(class ScriptRegistry::File* src);
    void revert();

    // the file from the registry being edited
    // this assumes File objects are interned and will not be deleted
    // for the duration of the application
    // they may however be marked missing
    ScriptRegistry::File* file = nullptr;
    
    void resized() override;
    
  private:

    class ScriptEditor* parent = nullptr;
    
    ScriptDetails details;
    CustomEditor editor;
    
};

class ScriptEditorTabButton : public juce::Component
{
  public:
    ScriptEditorTabButton(class ScriptEditor* se, ScriptEditorFile* f);
    ~ScriptEditorTabButton();
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override;
  private:
    class ScriptEditor* editor = nullptr;
    ScriptEditorFile* file = nullptr;
};

class ScriptEditor : public juce::Component, public juce::Button::Listener
{
  public:

    ScriptEditor();
    ~ScriptEditor();

    void resized() override;
    void buttonClicked(juce::Button* b) override;
    
    void load(class ScriptRegistry::File* file);

    void close(ScriptEditorFile* f);
    
    ScriptEditorFile* getCurrentFile();
    void newFile();
    void cancel();
    void compile();
    void revert();
    void save();

  private:

    BasicTabs tabs;
    BasicButtonRow buttons;
    juce::TextButton saveButton {"Save"};
    juce::TextButton compileButton {"Compile"};
    juce::TextButton revertButton {"Revert"};
    juce::TextButton newButton {"New"};
    juce::TextButton cancelButton {"Cancel"};
    
    juce::OwnedArray<ScriptEditorFile> files;
    juce::OwnedArray<ScriptRegistry::File> newFiles;
    
    void addTab(ScriptEditorFile* efile);
};

