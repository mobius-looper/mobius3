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

    bool hasFile(class ScriptRegistry::File* src);
    void refresh(class ScriptRegistry::File* src);
    
    void resized() override;

    ScriptRegistry::File* getFile();
    
  private:

    class ScriptEditor* parent = nullptr;
    std::unique_ptr<ScriptRegistry::File> ownedFile;
    
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
    
};

