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
#include "ScriptLog.h"
#include "CustomEditor.h"

//////////////////////////////////////////////////////////////////////
//
// File
//
//////////////////////////////////////////////////////////////////////

/**
 * Represents one file loaded into the editor.
 */
class ScriptEditorFile : public juce::Component
{
  public:

    ScriptEditorFile(class Supervisor* s, class ScriptEditor* e, class ScriptRegistry::File* src);
    ScriptEditorFile(class Supervisor* s, class ScriptEditor* e);
    ~ScriptEditorFile();

    void refresh(class ScriptRegistry::File* src);
    void compile();
    void revert();
    void save();
    void deleteFile();

    void resized() override;

    // the file from the registry being edited
    // this assumes File objects are interned and will not be deleted
    // for the duration of the application
    // they may however be marked missing
    // why is this public??
    ScriptRegistry::File* file = nullptr;

    void setTabIndex(int index) {
        tabIndex = index;
    }
    int getTabIndex() {
        return tabIndex;
    }
    
  private:

    class Supervisor* supervisor = nullptr;
    class ScriptEditor* parent = nullptr;
    int tabIndex = -1;
    
    ScriptDetails detailsHeader;
    CustomEditor editor;
    ScriptLog log;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastFolder;

    juce::String newName;
    std::unique_ptr<class ScriptRegistry::File> newFile;
    
    void logDetails(class MslDetails* details);
    void logError(class MslError* err, bool isError);
    void logCollision(class MslCollision* col);
    void logUnresolved(class MslDetails* details);
    void logError(juce::String text);
    
    void startSaveNew();
    void finishSaveNew(juce::File file);

    void startDeleteFile();
    void finishDeleteFile(int button);
    
};

//////////////////////////////////////////////////////////////////////
//
// TabButton
//
//////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////
//
// Editor
//
//////////////////////////////////////////////////////////////////////

class ScriptEditor : public juce::Component, public juce::Button::Listener
{
  public:

    ScriptEditor(class Supervisor* s);
    ~ScriptEditor();

    void resized() override;
    void buttonClicked(juce::Button* b) override;
    
    void load(class ScriptRegistry::File* file);

    void close(ScriptEditorFile* f);
    
    ScriptEditorFile* getCurrentFile();
    void newFile();
    void deleteFile();
    void compile();
    void revert();
    void save();

    void changeTabName(int index, juce::String name);
    void forceResize();
    
  private:

    class Supervisor* supervisor = nullptr;

    BasicTabs tabs;
    BasicButtonRow buttons;
    juce::TextButton compileButton {"Compile"};
    juce::TextButton revertButton {"Revert"};
    juce::TextButton saveButton {"Save"};
    juce::TextButton newButton {"New"};
    juce::TextButton deleteButton {"Delete"};
    
    juce::OwnedArray<ScriptEditorFile> files;
    
    void addTab(ScriptEditorFile* efile);
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
