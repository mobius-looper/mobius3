
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
    buttons.add(&newButton);
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
    buttons.setBounds(area.removeFromBottom(24));
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
        addTab(neu);
    }
}

void ScriptEditor::addTab(ScriptEditorFile* efile)
{
    juce::String name = efile->getFile()->name;
    if (name.length() == 0)
      name = "New";

    tabs.add(name, efile);

    // show the one we just added
    int index = tabs.getNumTabs() - 1;
    tabs.show(index);

    // add a close button
    juce::TabBarButton* tbb = tabs.getTabbedButtonBar().getTabButton(index);
    tbb->setExtraComponent(new ScriptEditorTabButton(this, efile), juce::TabBarButton::afterText);
}

void ScriptEditor::close(ScriptEditorFile* efile)
{
    // find the tab it is in
    ScriptEditorFile* found = nullptr;
    int index = 0;
    for (auto f : files) {
        if (f == efile) {
            found = f;
            break;
        }
        index++;
    }

    if (found != nullptr) {
        // todo: see if it was modified and prompt
        tabs.removeTab(index);
        files.removeObject(found, false);
        delete found;

        // removing a tab doesn't appear to auto select a different tab
        int ntabs = tabs.getNumTabs();
        if (index >= ntabs)
          index = ntabs - 1;
        tabs.show(index);
    }
}

void ScriptEditor::buttonClicked(juce::Button* b)
{
    if (b == &saveButton) {
        save();
    }
    else if (b == &compileButton) {
        compile();
    }
    else if (b == &revertButton) {
        revert();
    }
    else if (b == &newButton) {
        newFile();
    }
    else if (b == &cancelButton) {
        cancel();
    }
}

ScriptEditorFile* ScriptEditor::getCurrentFile()
{
    ScriptEditorFile* efile = nullptr;
    int index = tabs.getCurrentTabIndex();
    if (index >= 0)
      efile = files[index];
    return efile;
}

void ScriptEditor::newFile()
{
    // fake up a registry file so new and existing can work close to the same
    ScriptRegistry::File* rfile = new ScriptRegistry::File();
    rfile->isNew = true;
    newFiles.add(rfile);

    ScriptEditorFile* efile = new ScriptEditorFile(this, rfile);
    files.add(efile);
    addTab(efile);
}

void ScriptEditor::cancel()
{
    int index = tabs.getCurrentTabIndex();
    if (index >= 0) {
        ScriptRegistry::File* tempFile = nullptr;
        ScriptEditorFile* efile = files[index];
        if (efile->getFile()->isNew)
          tempFile = efile->getFile();
        
        // dispose of the editry, this also makes efile invalide
        close(efile);

        // and the temporary
        if (tempFile != nullptr)
          newFiles.removeObject(tempFile, true);
    }
}

void ScriptEditor::compile()
{
    ScriptEditorFile* efile = getCurrentFile();
    if (efile != nullptr) {
    }
}

void ScriptEditor::revert()
{
    ScriptEditorFile* efile = getCurrentFile();
    if (efile != nullptr) {
        efile->revert();
    }
    
}

void ScriptEditor::save()
{
    ScriptEditorFile* efile = getCurrentFile();
    if (efile != nullptr) {
    }
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

ScriptRegistry::File* ScriptEditorFile::getFile()
{
    return ownedFile.get();
}

/**
 * Here we were initialized once with the same path, but the
 * contents may have changed.
 *
 * Really HATING how File has to be copied.
 */
void ScriptEditorFile::refresh(ScriptRegistry::File* src)
{
    ownedFile.reset(src->cloneForEditor());
    
    details.load(ownedFile.get());

    if (ownedFile->unit != nullptr) {
        editor.setText(ownedFile->source);
    }
    else if (ownedFile->old) {
        juce::File f (ownedFile->path);
        juce::String source = f.loadFileAsString();
        editor.setText(source);
    }
}

void ScriptEditorFile::revert()
{
    if (ownedFile != nullptr)
      editor.setText(ownedFile->source);
}

bool ScriptEditorFile::hasFile(ScriptRegistry::File* src)
{
    return ownedFile->path == src->path;
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
