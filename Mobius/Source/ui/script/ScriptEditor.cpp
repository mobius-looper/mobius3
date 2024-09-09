
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../Supervisor.h"

#include "../../script/ScriptRegistry.h"
#include "../../script/MslEnvironment.h"
#include "../../script/MslDetails.h"
#include "../../script/MslError.h"
#include "../../script/MslCollision.h"
#include "../../script/MslLinkage.h"

#include "ScriptEditor.h"

//////////////////////////////////////////////////////////////////////
//
// ScriptEditorFile
//
//////////////////////////////////////////////////////////////////////

/**
 * If a registry file pointer is passed, this is an existing file.
 * The file object is assumed to be interned and will live as long as the
 * editor does.
 *
 * If the file is nullptr, this is being called by ScriptEditor in response to
 * the "New" button.  Fake up a transient file object and make it look like
 * it came from the regsitry.
 */
ScriptEditorFile::ScriptEditorFile(Supervisor* s, ScriptEditor* e, ScriptRegistry::File* src)
{
    supervisor = s;
    parent = e;
    
    addAndMakeVisible(detailsHeader);
    addAndMakeVisible(editor);
    addAndMakeVisible(log);
    
    // todo: get this from UIConfig someday
    editor.setEmacsMode(true);

    // we'll show errors in the log do this doesn't have to
    detailsHeader.setIncludeExtra(false);
    detailsHeader.setIncludeErrors(false);

    // fake up a registry file so new and existing can work close to the same
    if (src == nullptr) {
        src = new ScriptRegistry::File();
        src->path = "<not saved>";
        newFile.reset(src);
    }
    file = src;
    
    refresh(file);
}

ScriptEditorFile::~ScriptEditorFile()
{
}

void ScriptEditorFile::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    detailsHeader.setBounds(area.removeFromTop(detailsHeader.getPreferredHeight()));
    log.setBounds(area.removeFromBottom(100));
    editor.setBounds(area);
}

void ScriptEditorFile::logError(juce::String text)
{
    log.setColour(juce::TextEditor::ColourIds::textColourId, juce::Colours::red);
    log.add(text);
    log.setColour(juce::TextEditor::ColourIds::textColourId, juce::Colours::white);
}

/**
 * Here we were initialized once with the same path, but the
 * contents may have changed.
 *
 * File objects are interned so we should have the same
 * object in both places.
 */
void ScriptEditorFile::refresh(ScriptRegistry::File* src)
{
    if (file != src)
      Trace(1, "ScriptEditor: File internment seems to be broken");
    
    if (newFile != nullptr) {
        // we're editing a temp, don't need to pull anything from the src
    }
    else if (file->source.length() == 0) {
        // ScriptClerk isnt maintaining the source code for old .mos files
        // it should...
        juce::File f (file->path);
        file->source = f.loadFileAsString();
    }
    
    // this can show errors, but we ask it not to
    detailsHeader.setNameOverride("");
    detailsHeader.load(file);
    editor.setText(file->source);
    logDetails(file->getDetails());
}

void ScriptEditorFile::revert()
{
    if (file != nullptr)
      editor.setText(file->source);

    // clear modification tracking once we have it
    log.clear();
    // todo: script metadata set in directives is more than just the
    // name, once you start adding author or whatever, will need pending
    // changes for that too
    detailsHeader.setNameOverride("");
}

void ScriptEditorFile::compile()
{
    MslEnvironment* env = supervisor->getMslEnvironment();
    MslDetails* result = env->compile(supervisor, editor.getText());

    log.clear();
    if (result != nullptr) {
        if (!result->hasErrors())
          log.add("Compile successful");
        else
          logDetails(result);

        // if we compiled a #name directive, it's nice to show that
        // in the details header so they can see that it worked
        if (result->name.length() > 0 && result->name != file->name) {
            detailsHeader.setNameOverride(result->name);
            detailsHeader.repaint();
        }
        
        delete result;
    }
}

void ScriptEditorFile::logDetails(MslDetails* details)
{
    log.clear();
    if (details != nullptr) {
        for (auto err : details->errors) logError(err, true);
        for (auto err : details->warnings) logError(err, false);
        for (auto col : details->collisions) logCollision(col);
        logUnresolved(details);
    }
}

void ScriptEditorFile::logError(MslError* err, bool isError)
{
    if (isError) {
        log.setColour(juce::TextEditor::ColourIds::textColourId, juce::Colours::red);
        log.append("Error: ");
    }
    else {
        log.setColour(juce::TextEditor::ColourIds::textColourId, juce::Colours::yellow);
        log.append("Warning: ");
    }
    log.setColour(juce::TextEditor::ColourIds::textColourId, juce::Colours::grey);
    
    if (err->source == MslSourceCompiler || err->source == MslSourceLinker) {
        log.append("line " + juce::String(err->line) + " column " + juce::String(err->column));
        // for unresolved doesn't really add anything and looks busy, but
        // it's better for other things
        // would be nice to have a few error types to taylor rendering
        log.append(" token ");
        log.setColour(juce::TextEditor::ColourIds::textColourId, juce::Colours::white);
        log.append(err->token);
        log.setColour(juce::TextEditor::ColourIds::textColourId, juce::Colours::grey);
        log.append(" : ");
    }
    log.setColour(juce::TextEditor::ColourIds::textColourId, juce::Colours::white);
    log.add(err->details);
}

void ScriptEditorFile::logCollision(MslCollision* col)
{
    log.setColour(juce::TextEditor::ColourIds::textColourId, juce::Colours::red);
    log.append("Name collision: ");
    log.setColour(juce::TextEditor::ColourIds::textColourId, juce::Colours::white);
    log.append(col->name);
    log.setColour(juce::TextEditor::ColourIds::textColourId, juce::Colours::grey);
    log.append(" with file ");
    log.setColour(juce::TextEditor::ColourIds::textColourId, juce::Colours::white);
    log.add(col->otherPath);
}

void ScriptEditorFile::logUnresolved(MslDetails* details)
{
    // kludge: when compiling we'll also have warnings in the warning list for
    // every unresolved symbol, when looking at post-install link errors we may not
    // really?  won't the warnings always be there?
    // peek into the warning list and if we have warnings starting with "Unresolved"
    // suppress the redundant unresolved name dump
    if (details != nullptr && details->unresolved.size() > 0) {

        bool hasWarnings = false;
        for (auto err : details->warnings) {
            if (juce::String(err->details).startsWith("Unresolved")) {
                hasWarnings = true;
                break;
            }
        }

        if (!hasWarnings) {
            log.setColour(juce::TextEditor::ColourIds::textColourId, juce::Colours::yellow);
            log.append("Unresolved symbols: ");
            log.setColour(juce::TextEditor::ColourIds::textColourId, juce::Colours::white);
            log.add(details->unresolved.joinIntoString(","));
        }
    }
}

/**
 * Save is complex...
 *
 * If this contains an existin file, it write the new file content and installs it.
 *
 * If this is a new file there are two options:
 *
 *    - pop up the usual Save As file browser and make them navigate to a location
 *      and enter a file name
 *
 *    - save the file directly into the registry without a file browser, using the
 *      name declared with the #name directive
 *
 * As people become used to using the library, the need for file browsers diminishes
 * and gets in the way.  But, if the #name already matches a leaf file name from
 * another unit there will be a path collision.  As sharing becomes more common
 * it would be really nice if files could have generated names so you don't have to worry
 * about path collisions.  Really paths should become almost invisible, you only use #name
 *
 * Work on that...for now use a file browser.
 */
void ScriptEditorFile::save()
{
    if (newFile != nullptr) {
        startSaveNew();
    }
    else {
        // put the new source on the File and ask Clerk to save it
        file->source = editor.getText();
        ScriptClerk* clerk = supervisor->getScriptClerk();
        bool saved = clerk->saveFile(parent, file);
        if (!saved) {
            // this means there was something wrong with actually touching
            // the file, which would have prevented installation
            logError("File save failed");
        }
        else {
            // details will have been refreshed
            MslDetails* details = file->getDetails();
            if (!details->hasErrors())
              log.add("Save successful");
            logDetails(details);
        }
    }
}

void ScriptEditorFile::startSaveNew()
{
    juce::File startPath(supervisor->getRoot().getChildFile("scripts"));
    if (lastFolder.length() > 0)
      startPath = lastFolder;

    juce::String title = "Select a script destination...";

    chooser = std::make_unique<juce::FileChooser> (title, startPath, "*.msl");

    auto chooserFlags = juce::FileBrowserComponent::saveMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::warnAboutOverwriting;

    // !!!!!!!!!!
    // is it extremely dangerous to capture "this" because there is no assurance
    // that the user won't delete this tab while the browser is active
    // experiment with using a SafePointer or just passing some kind of unique
    // identifier and Supervisor, and letting Supervisor walk back down
    // to the ScriptEditorFile if it still exists
    
    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        // magically get here after the modal dialog closes
        // the array will be empty if Cancel was selected
        juce::Array<juce::File> result = fc.getResults();
        if (result.size() > 0) {
            // chooserFlags should have only allowed one
            juce::File file = result[0];
            
            finishSaveNew(file);

            // remember this directory for the next time
            lastFolder = file.getParentDirectory().getFullPathName();
        }
        
    });
}

/**
 * Basically the same as save() with different way of getting there
 * refactor...
 */
void ScriptEditorFile::finishSaveNew(juce::File dest)
{
    file->path = dest.getFullPathName();
    file->source = editor.getText();

    ScriptClerk* clerk = supervisor->getScriptClerk();
    bool saved = clerk->addFile(parent, file);
    if (!saved) {
        logError("File save failed");
    }
    else {
        // ownership of the File has transferred
        (void)newFile.release();

        MslDetails* details = file->getDetails();
        if (!details->hasErrors())
          log.add("Save successful");
        logDetails(details);

        // compiling will have derived a reference name, refresh the header
        parent->changeTabName(tabIndex, file->name);
        detailsHeader.repaint();
    }
}

/**
 * Delete is complex...
 *
 * If this is a new file there isn't much to do beyond self-closing the tab
 * Should this even be handled down here or should ScriptEditor be in charge?
 *
 * If changes were made, popup an "are you sure" dialog.
 *
 * If the file exists, delete it first then update the registry.
 * Like save() this logic should be moved to ScriptClerk once it is working.
 */
void ScriptEditorFile::deleteFile()
{
    if (newFile != nullptr) {
        parent->close(this);
    }
    else {
        startDeleteFile();
    }
}

void ScriptEditorFile::startDeleteFile()
{
    // !!!!!!!!!!
    // like startSaveNew is it extremely dangerous to capture "this" because there is no assurance
    // that the user won't delete this tab while the browser is active
    // experiment with using a SafePointer or just passing some kind of unique
    // identifier and Supervisor, and letting Supervisor walk back down
    // to the ScriptEditorFile if it still exists
    
    // launch an async dialog box that calls the lambda when finished
    juce::MessageBoxOptions options = juce::MessageBoxOptions()
        .withIconType (juce::MessageBoxIconType::WarningIcon)
        .withTitle ("Deleting Script File")
        .withMessage ("Are you sure you want to permanently delete this file?\n" +
                      file->path)
        .withButton ("Delete")
        .withButton ("Cancel")
        //.withAssociatedComponent (myComp)
        ;
        
    juce::AlertWindow::showAsync (options, [this] (int button) {
        finishDeleteFile(button);
    });
}

void ScriptEditorFile::finishDeleteFile(int button)
{
    if (button == 1) {

        ScriptClerk* clerk = supervisor->getScriptClerk();
        bool deleted = clerk->deleteFile(parent, file);
        if (!deleted) {
            logError("File delete failed");
        }
        else {
            // not much should go wrong during the uninstall
            // this could cause unresolved in other scripts
            // but that won't prevent closing the tab
            // if there are any serious errors, keep the tab open
            MslDetails* details = file->getDetails();
            if (details->hasErrors())
              logDetails(details);
            else
              parent->close(this);
        }
    }   
}

//////////////////////////////////////////////////////////////////////
// 
// ScriptEditor (outer window)
//
//////////////////////////////////////////////////////////////////////

ScriptEditor::ScriptEditor(Supervisor* s)
{
    supervisor = s;
    addAndMakeVisible(tabs);
    
    buttons.setListener(this);
    buttons.setCentered(true);
    buttons.add(&compileButton);
    buttons.add(&revertButton);
    buttons.add(&saveButton);
    buttons.add(&newButton);
    buttons.add(&deleteButton);
    addAndMakeVisible(buttons);

    // unlike most things that add/remove listeners when they are shown and hidden
    // we don't have show/hide hooks, just leave the listener on all the time
    // but ignore the callbacks if we're not visible
    ScriptClerk* clerk = s->getScriptClerk();
    clerk->addListener(this);
}

ScriptEditor::~ScriptEditor()
{
    ScriptClerk* clerk = supervisor->getScriptClerk();
    clerk->removeListener(this);
}

void ScriptEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    (void)area.removeFromBottom(4);
    buttons.setBounds(area.removeFromBottom(24));
    tabs.setBounds(area);
}

/**
 * What uniquely identifies a file is the path.
 * The name may have collisions which is why we're here to fix it.
 * This can result in two tabs with the same name.  Should be
 * coloring them to indiciate this.
 *
 * Actually, now that Files are interned, can just compare pointers too.
 */
void ScriptEditor::load(ScriptRegistry::File* file)
{
    ScriptEditorFile* existing = nullptr;
    for (auto efile : files) {
        if (efile->file->path == file->path) {
            existing = efile;
            break;
        }
    }

    if (existing != nullptr) {
        existing->refresh(file);
    }
    else {
        ScriptEditorFile* neu = new ScriptEditorFile(supervisor, this, file);
        files.add(neu);
        addTab(neu);
    }
}

void ScriptEditor::addTab(ScriptEditorFile* efile)
{
    juce::String name = efile->file->name;
    if (name.length() == 0)
      name = "New";

    tabs.add(name, efile);

    // show the one we just added
    int index = tabs.getNumTabs() - 1;
    tabs.show(index);

    // remember it so we can get back easily
    efile->setTabIndex(index);

    // add a close button
    juce::TabBarButton* tbb = tabs.getTabbedButtonBar().getTabButton(index);
    tbb->setExtraComponent(new ScriptEditorTabButton(this, efile), juce::TabBarButton::afterText);
}

/**
 * Called by ScriptEditorFile when it wants to change the name of a tab
 */
void ScriptEditor::changeTabName(int index, juce::String name)
{
    juce::TabBarButton* tbb = tabs.getTabbedButtonBar().getTabButton(index);
    tbb->setButtonText(name);

    // note: just changing the button text doesn't resize the button so
    // it starts out "New" which is narrow, and usually gets truncated after
    // renaming.
    // none of these worked, dragging the size of the containing window
    // works but is not automatic, there must be a way...
    // just resized() didn't work
    //resized();
    //repaint();
    //setBounds(getLocalBounds());
}

void ScriptEditor::close(ScriptEditorFile* efile)
{
    // todo: see if it was modified and prompt
    
    int index = efile->getTabIndex();
    tabs.removeTab(index);
    files.removeObject(efile, true);

    // removing a tab doesn't appear to auto select a different tab
    int ntabs = tabs.getNumTabs();
    if (index >= ntabs)
      index = ntabs - 1;
    tabs.show(index);
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
    else if (b == &deleteButton) {
        deleteFile();
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
    ScriptEditorFile* efile = new ScriptEditorFile(supervisor, this, nullptr);
    files.add(efile);
    addTab(efile);
}

void ScriptEditor::compile()
{
    ScriptEditorFile* efile = getCurrentFile();
    if (efile != nullptr) {
        efile->compile();
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
        efile->save();
    }
}

void ScriptEditor::deleteFile()
{
    ScriptEditorFile* efile = getCurrentFile();
    if (efile != nullptr)
      efile->deleteFile();
}

//
// ScriptClerk::Listener overrides
//

/**
 * ScriptConfigEditor can't modify files so this shouldn't be triggered
 */
void ScriptEditor::scriptFileSaved(class ScriptRegistry::File* file)
{
    (void)file;
    Trace(1, "ScriptEditor::scriptFileSaved Why is this being called?");
}

/**
 * ScriptConfigEditor can add new Externals, so this would be triggered
 * Since the editor doesn't track new files it can be ignored
 */
void ScriptEditor::scriptFileAdded(class ScriptRegistry::File* file)
{
    (void)file;
}

/**
 * ScriptConfigEditor can delete Externals
 * If we have a tab open for one, close it.
 *
 * We could "are you sure?" if there are unsaved edits on this file.
 * It will still have been removed from the external file list but
 * at least we could save what was being done.
 *
 * Note that since we're permanently installed as a clerk listener,
 * we'll be called even when we're not visible.
 */
void ScriptEditor::scriptFileDeleted(class ScriptRegistry::File* file)
{
    (void)file;
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
