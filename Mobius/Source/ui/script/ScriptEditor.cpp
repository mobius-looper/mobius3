
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../script/ScriptRegistry.h"

#include "ScriptEditor.h"

ScriptEditor::ScriptEditor()
{
    addAndMakeVisible(tabs);

    buttons.setListener(this);
    buttons.setCentered(true);
    buttons.add(&saveButton);
    buttons.add(&compileButton);
    buttons.add(&revertButton);
    buttons.add(&cancelButton);
    addAndMakeVisible(buttons);
}

ScriptEditor::~ScriptEditor()
{
}

void ScriptEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    (void)area.removeFromBottom(4);
    buttons.setBounds(area.removeFromBottom(20));
    tabs.setBounds(getLocalBounds());
}

/**
 * What uniquely identifies a file is the path.
 * The name may have collisions which is why we're here to fix it.
 * This can result in two tabs with the same name.  Should be
 * coloring them to indiciate this.
 */
void ScriptEditor::load(ScriptRegistry::File* file)
{
    ScriptEditorFile* existing = nullptr;
    for (auto efile : files) {
        if (efile->hasFile(file)) {
            existing = efile;
            break;
        }
    }

    if (existing != nullptr) {
        existing->refresh(file);
    }
    else {
        ScriptEditorFile* neu = new ScriptEditorFile(this, file);
        files.add(neu);
        tabs.add(file->name, neu);

        // show the one we just added
        int index = tabs.getNumTabs() - 1;
        tabs.show(index);

        // add a close button
        juce::TabBarButton* tbb = tabs.getTabbedButtonBar().getTabButton(index);
        tbb->setExtraComponent(new ScriptEditorTabButton(this, neu), juce::TabBarButton::afterText);
    }
}

void ScriptEditor::close(ScriptEditorFile* file)
{
    // find the tab it is in
    ScriptEditorFile* found = nullptr;
    int index = 0;
    for (auto f : files) {
        if (f == file) {
            found = f;
            break;
        }
        index++;
    }

    if (found != nullptr) {
        // todo: see if it was modified and prompt
        tabs.removeTab(index);
        files.removeObject(file, false);
        delete file;

        // removing a tab doesn't appear to auto select a different tab
        int ntabs = tabs.getNumTabs();
        if (index >= ntabs)
          index = ntabs - 1;
        tabs.show(index);
    }
}

void ScriptEditor::buttonClicked(juce::Button* b)
{
    (void)b;
}

//////////////////////////////////////////////////////////////////////
//
// ScriptEditorTabButton
//
// Adapted from the WidgetsDemo
//
//////////////////////////////////////////////////////////////////////

ScriptEditorTabButton::ScriptEditorTabButton(ScriptEditor* se, ScriptEditorFile* f)
{
    editor = se;
    file = f;
    setSize(14, 14);
}

ScriptEditorTabButton::~ScriptEditorTabButton()
{
}

void ScriptEditorTabButton::paint(juce::Graphics& g)
{
    juce::Path star;
    // Point center, numberOfPoints, innerRadius, outerRadius, startAngle
    // don't understand what these means and the docs are sparse
    // increasing the outer radius makes it thinner
    // default start is a vertical cross, rotating .5 tilts it but not quite 45 degrees
    // .8 gets it pretty close to an X
    star.addStar ({}, 4, 1.0f, 4.0f, 0.80f);

    g.setColour (juce::Colours::darkred);
    g.fillPath (star, star.getTransformToScaleToFit (getLocalBounds().reduced (2).toFloat(), true));
}

/**
 * This is going to delete the world out from under this method.
 * Would be safer to post a message rather than have this delete itself?
 */
void ScriptEditorTabButton::mouseDown(const juce::MouseEvent& e)
{
    (void)e;
    editor->close(file);
}

//////////////////////////////////////////////////////////////////////
//
// ScriptEditorFile
//
//////////////////////////////////////////////////////////////////////

/**
 * The lifespan of the File is not guaranteed.
 * If the registry refreshes and detects deleted files, a File may
 * be deleted.  The lifetime of the MslScriptUnit IS guaranteed for
 * the duration of the application.
 *
 * So the File contents must be copied
 */
ScriptEditorFile::ScriptEditorFile(ScriptEditor* e, ScriptRegistry::File* src)
{
    parent = e;
    
    addAndMakeVisible(details);
    addAndMakeVisible(editor);

    // todo: get this from UIConfig someday
    editor.setEmacsMode(true);

    refresh(src);
}

ScriptEditorFile::~ScriptEditorFile()
{
}

/**
 * Here we were initialized once with the same path, but the
 * contents may have changed.
 *
 * Really HATING how File has to be copied.
 */
void ScriptEditorFile::refresh(ScriptRegistry::File* src)
{
    file.reset(src->cloneForEditor());
    
    details.load(file.get());

    if (file->unit != nullptr) {
        editor.setText(file->unit->source);
    }
    else if (file->old) {
        juce::File f (file->path);
        juce::String source = f.loadFileAsString();
        editor.setText(source);
    }
}

bool ScriptEditorFile::hasFile(ScriptRegistry::File* src)
{
    return file->path == src->path;
}

void ScriptEditorFile::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    details.setBounds(area.removeFromTop(details.getPreferredHeight()));
    editor.setBounds(area);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
