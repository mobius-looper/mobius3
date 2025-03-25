/**
 * All binding panels share a common structure
 * They are ConfigPanels so have Save/Cancel buttons in the footer.
 * They have an optional object selector for bindings that have more than one object.
 *
 * On the left is a large scrolling binding table with columns for
 *
 *    Target
 *    Trigger
 *    Scope
 *    Arguments
 * 
 * Under the Target table are buttons New, Update, Delete to manage rows in the table.
 *
 * Under the BindingTargetSelector are Extended Fields to add additional information about
 * the Binding.  At minimum it will have an Arguments field to specifify arbitrary
 * trigger.
 *
 * todo: with the introduction of Symbols, an existing binding may be "unresolved"
 * if it has a name that does not correspond to a resolved Symbol.  Need to
 * display those in red or something.
 *
 */

#include <JuceHeader.h>

#include <string>
#include <sstream>

#include "../../util/Trace.h"
#include "../../model/SystemConfig.h"
#include "../../model/BindingSets.h"
#include "../../model/BindingSet.h"
#include "../../model/Binding.h"
#include "../../model/Scope.h"
#include "../../model/GroupDefinition.h"
#include "../JuceUtil.h"
#include "../MobiusView.h"

// temporary until we can get the initialization order sorted out
#include "../../Supervisor.h"

#include "OldBindingTable.h"
#include "OldBindingTargetSelector.h"

#include "OldBindingEditor.h"

OldBindingEditor::OldBindingEditor(Supervisor* s) : ConfigEditor(s), targets(s)
{
    setName("OldBindingEditor");
    
    // this one is selectively shown
    addChildComponent(activationButtons);
    
    //activeButton.setColour(juce::ToggleButton::ColourIds::textColourId, juce::Colours::white);
    //activeButton.setColour(juce::ToggleButton::ColourIds::tickColourId, juce::Colours::red);
    //activeButton.setColour(juce::ToggleButton::ColourIds::tickDisabledColourId, juce::Colours::white);
    //activationButtons.add(&activeButton);
    
    overlayButton.setColour(juce::ToggleButton::ColourIds::textColourId, juce::Colours::white);
    overlayButton.setColour(juce::ToggleButton::ColourIds::tickColourId, juce::Colours::red);
    overlayButton.setColour(juce::ToggleButton::ColourIds::tickDisabledColourId, juce::Colours::white);
    activationButtons.add(&overlayButton);
    
    bindings.setListener(this);
    addAndMakeVisible(bindings);

    addAndMakeVisible(targets);
    targets.setListener(this);

    addAndMakeVisible(form);
}

OldBindingEditor::~OldBindingEditor()
{
}

void OldBindingEditor::setInitialObject(juce::String name)
{
    initialObject = name;
}

/**
 * ConfigPanel overload to prepare the panel to be shown.
 * Make copies of all the BindingSets in bindingSets and revertBindingSets.
 * Load the first BindingSet into the BindingTable.
 *
 * As the form is edited, changes are made to the model in the TABLE,
 * not the model that is in the bindingSets array.  This is unlike
 * Preset and others where modifications are made directly into the OldBindingEditor
 * object list.  This means we have two copies of Bindings and you need to
 * be careful about which is used.
 */
void OldBindingEditor::load()
{
    MobiusView* view = supervisor->getMobiusView();

    maxTracks = view->totalTracks;
    //maxGroups = config->groups.size();

    refreshScopeNames();
    targets.load();

    // Though only MidiPanel supports overlays, handle all three
    // the same.  ButtonPanel overloads this differently

    bindingSets.clear();
    revertBindingSets.clear();

    SystemConfig* scon = supervisor->getSystemConfig();
    BindingSets* container = scon->getBindings();
    
    // bootstrap a base set if this is a fresh install with nothing
    (void)container->getBase();

    // copy all the BindingSets in the source
    juce::OwnedArray<BindingSet>& sets = container->getSets();
    for (auto src : sets) {

        BindingSet* set = new BindingSet(src);
        bindingSets.add(set);
        revertBindingSets.add(new BindingSet(set));
    }

    if (initialObject.length() > 0) {
        // this is the first time here for an editor that supports
        // multiple binding sets, which is really only MidiEditor
        // pre-select this one since it is likely it will be the first
        // one to be edited
        selectedBindingSet = 0;
        for (int i = 0 ; i < bindingSets.size() ; i++) {
            if (bindingSets[i]->name == initialObject) {
                selectedBindingSet = i;
                break;
            }
        }
        // only do this the first time
        initialObject = "";
    }
    else {
        // on subseqeuent opens, maintain the last selection unless an object
        // got lost for some reason
        if (selectedBindingSet >= bindingSets.size())
          selectedBindingSet = 0;
    }
    
    // make another copy of the Binding list into the table
    loadBindingSet(selectedBindingSet);

    refreshObjectSelector();
        
    resetFormAndTarget();
}

/**
 * Refresh the object selector on initial load and after any
 * objects are added or removed.  This could be pushed up to
 * ConfigPanel if each subclass had a method to return
 * the list of names and the current selection, but at that point
 * you're not eliminating much duplication.
 */
void OldBindingEditor::refreshObjectSelector()
{
    juce::Array<juce::String> names;
    for (auto set : bindingSets) {
        if (set->name.length() == 0)
          set->name = "[New]";
        names.add(set->name);
    }
    context->setObjectNames(names);
    context->setSelectedObject(selectedBindingSet);
}

void OldBindingEditor::loadBindingSet(int index)
{
    bindings.clear();
    BindingSet* set = bindingSets[index];
    if (set != nullptr) {
        for (auto binding : set->getBindings()) {
            // subclass overload
            if (isRelevant(binding)) {
                // table will copy
                bindings.add(binding);
            }
        }
    }
    bindings.updateContent();
    resetFormAndTarget();

    // this is shown only when editing one of the overlay sets
    // hmm, think about adding an "active" here too, if you're editing
    // update: no longer doing activation here, there is only the "overlay" flag
    activationButtons.setVisible(index > 0);
    //activeButton.setToggleState(set->isActive(), juce::NotificationType::dontSendNotification);
    overlayButton.setToggleState(set->overlay, juce::NotificationType::dontSendNotification);
}

/**
 * Called by the Save button in the footer.
 * 
 * Save all presets that have been edited during this session
 * back to the master configuration.
 */
void OldBindingEditor::save()
{
    // capture visible state in the table back into
    // the current BidningSet
    saveBindingSet(selectedBindingSet);
        
    // build a new BindingSets container
    BindingSets* newContainer = new BindingSets();
    while (bindingSets.size() > 0) {
        newContainer->add(bindingSets.removeAndReturn(0));
    }

    // these we don't need any more
    revertBindingSets.clear();

    supervisor->bindingEditorSave(newContainer);
}

/**
 * Merge rules:
 *
 * Any binding set with merge=true has an independent
 * active flag which may be on or off.
 *
 * Any binding set with merge=false, when made active,
 * turns off active any all other binding sets that
 * do not have the merge flag.
 */
void OldBindingEditor::saveBindingSet(int index)
{
    BindingSet* set = bindingSets[index];
    if (set != nullptr) {
        saveBindingSet(set);
        if (index > 0) {
            set->overlay = overlayButton.getToggleState();
        }
    }
}

/**
 * Take the set of Binding objects that have been edited in the Binding
 * table and merge them back into a BindingSet.  The BindingTable
 * only held a subset of the Bindings that were in the BindingSet
 * so everything that wasn't in the table needs to be preserved,
 * and everything that was copied to the table neds to be replaced.
 */
void OldBindingEditor::saveBindingSet(BindingSet* dest)
{
    // remove any of the potentially edited bindings from the list
    // one of the rare times we do surgery on the list
    juce::OwnedArray<Binding>& original = dest->getBindings();
    int index = 0;
    while (index < original.size()) {
        Binding* b = original[index];
        if (isRelevant(b))
          original.remove(index, true);
        else
          index++;
    }

    // add back the edited ones, some may have been deleted
    // and some may be new
    juce::Array<Binding*> edited;
    bindings.captureBindings(edited);

    while (edited.size() > 0)
      original.add(edited.removeAndReturn(0));
}

/**
 * Throw away all editing state.
 */
void OldBindingEditor::cancel()
{
    // throw away the copies
    bindings.clear();
    
    // delete the copied sets
    bindingSets.clear();
    revertBindingSets.clear();
}

void OldBindingEditor::revert()
{
    BindingSet* revert = revertBindingSets[selectedBindingSet];
    if (revert != nullptr) {
        BindingSet* reverted = new BindingSet(revert);
        bindingSets.set(selectedBindingSet, reverted);
        loadBindingSet(selectedBindingSet);
        // in case the name was edited
        refreshObjectSelector();
    }
}

//////////////////////////////////////////////////////////////////////
//
// ObjectSelector overloads
//
// Okay,  this is now the fourth multi-object panel, and we duplicate
// the same logic in all three with different names.  Need to refactor
// generic logic back down to ConfigPanel with subclass overloads
// for the things that differ.
//
//////////////////////////////////////////////////////////////////////

/**
 * Called when the combobox changes.
 */
void OldBindingEditor::objectSelectorSelect(int ordinal)
{
    if (ordinal != selectedBindingSet) {
        saveBindingSet(selectedBindingSet);
        selectedBindingSet = ordinal;
        loadBindingSet(selectedBindingSet);
    }
}

void OldBindingEditor::objectSelectorNew(juce::String name)
{
    int newOrdinal = bindingSets.size();
    BindingSet* neu = new BindingSet();
    neu->name = "[New]";

    bindingSets.add(neu);
    // make another copy for revert
    BindingSet* revert = new BindingSet(neu);
    revertBindingSets.add(revert);
    
    selectedBindingSet = newOrdinal;
    loadBindingSet(selectedBindingSet);

    refreshObjectSelector();
}

/**
 * Delete is somewhat complicated.
 * You can't undo it unless we save it somewhere.
 * An alert would be nice, ConfigPanel could do that.
 */
void OldBindingEditor::objectSelectorDelete()
{
    // MidiPanel is unique in that the first one is reserved
    // and must always be there, it has to overload this

    if (bindingSets.size() == 1) {
        // must have at least one object
    }
    else {
        bindingSets.remove(selectedBindingSet);
        revertBindingSets.remove(selectedBindingSet);
        // leave the index where it was and show the next one,
        // if we were at the end, move back
        int newOrdinal = selectedBindingSet;
        if (newOrdinal >= bindingSets.size())
          newOrdinal = bindingSets.size() - 1;
        selectedBindingSet = newOrdinal;
        loadBindingSet(selectedBindingSet);
        refreshObjectSelector();
    }
}

void OldBindingEditor::objectSelectorRename(juce::String newName)
{
    BindingSet* set = bindingSets[selectedBindingSet];
    set->name = newName;
    // this doesn't need to refreshObjectSelector since that's
    // where the name came from
}

//////////////////////////////////////////////////////////////////////
//
// Trigger/Scope/Arguments Form
//
//////////////////////////////////////////////////////////////////////

/**
 * Build out the form containing scope, subclass specific fields, 
 * and binding arguments.
 *
 * Don't have a Form interface that allows static Field objects
 * so we have to allocate them and let the Form own them.
 */
void OldBindingEditor::initForm()
{
    // scope always goes first
    form.add(&scope);
    scope.setListener(this);
    refreshScopeNames();

    // subclass gets to add it's fields
    // it should always add "this" as the listener so we can
    // consistently end up in fieldChanged below and refresh
    // the BindingTable
    addSubclassFields();

    // arguments last
    form.add(&arguments);
    arguments.setListener(this);

    // subclass overloads this if it wants to use capture
    if (wantsCapture()) {
        form.add(&capture);
        annotation.setAdjacent(true);
        form.add(&annotation);
        if (wantsPassthrough()) {
            passthrough.setAdjacent(true);
            form.add(&passthrough);
        }
    }

    addAndMakeVisible(&form);
}

/**
 * Called by the subclass to add a release checkbox, normally
 * after another field.  Examples: MIDI type and Key
 */
void OldBindingEditor::addRelease()
{
    release.setAdjacent(true);
    form.add(&release);
}

/**
 * This needs to be done every time in order to track group renames.
 */
void OldBindingEditor::refreshScopeNames()
{
    // scope always goes first
    juce::StringArray scopeNames;
    scopeNames.add("Global");

    // context is not always set at this point so we have to go direct
    // to Supervisor to get to GroupDefinitions, this sucks work out a more
    // orderly initialization sequence

    MobiusView* view = supervisor->getMobiusView();
    maxTracks = view->totalTracks;
    for (int i = 0 ; i < maxTracks ; i++)
      scopeNames.add("Track " + juce::String(i+1));

    juce::StringArray gnames;
    GroupDefinitions* groups = supervisor->getGroupDefinitions();
    groups->getGroupNames(gnames);

    for (auto gname : gnames) {
        scopeNames.add(juce::String("Group ") + gname);
    }
    
    scope.setItems(scopeNames);
}

/**
 * Subclass calls back to see when capture is enabled.
 */
bool OldBindingEditor::isCapturing()
{
    return capture.getValue();
}

bool OldBindingEditor::isCapturePassthrough()
{
    return passthrough.getValue();
}

/**
 * Subclass back to this to show a string representation of
 * what is currently being monitored.  This happens whether
 * capture is on or off.
 *
 * todo: this could time out after awhile and go blank rather than
 * keeping the last capture forever
 */
void OldBindingEditor::showCapture(juce::String& s)
{
    annotation.setValue(s);

    // subclass will have already captured to the fields
    // here we can automatically update the binding as well
    if (isCapturing())
      formChanged();
}

void OldBindingEditor::yanInputChanged(YanInput* i)
{
    (void)i;
    formChanged();
}

void OldBindingEditor::yanComboSelected(YanCombo* c, int selection)
{
    (void)c;
    (void)selection;
    formChanged();
}

/**
 * Reset all trigger and target arguments to their initial state
 */
void OldBindingEditor::resetForm()
{
    scope.setSelection(0);
    release.setValue(false);
    
    resetSubclassFields();
    
    arguments.setValue("");
}

void OldBindingEditor::resetFormAndTarget()
{
    resetForm();
    targets.reset();
}

/**
 * Refresh form to have values for the selected binding
 *
 * Binding model represents scopes as a string, then parses
 * that into track or group numbers.  
 */
void OldBindingEditor::refreshForm(Binding* b)
{
    // if anything goes wrong parsing the scope string, leave the
    // selection at "Global"
    scope.setSelection(0);
    
    const char* scopeString = b->scope.toUTF8();
    int tracknum = Scope::parseTrackNumber(scopeString);
    if (tracknum > maxTracks) {
        // must be an old binding created before reducing
        // the track count, it reverts to global, should
        // have a more obvious warning in the UI
        Trace(1, "OldBindingEditor: Binding scope track number out of range %d", tracknum);
    }
    else if (tracknum >= 0) {
        // element 0 is "global" so track number works
        scope.setSelection(tracknum);
    }
    else {
        GroupDefinitions* groups = supervisor->getGroupDefinitions();
        int index = groups->getGroupIndex(b->scope);
        if (index >= 0)
          scope.setSelection(maxTracks + index);
        else
          Trace(1, "OldBindingEditor: Binding scope with unresolved group name %s", scopeString);
    }
    
    targets.select(b);
    refreshSubclassFields(b);
    
    arguments.setValue(b->arguments);
    release.setValue(b->release);
    
    // used this in old code, now that we're within a form still necessary?
    // arguments.repaint();
}

/**
 * Copy what we have displayed for targets, scopes, and arguments
 * into a Binding.
 *
 * Binding currently wants scopes represented as a String
 * with tracks as numbers "1", "2", etc. and groups as
 * letters "A", "B", etc.
 *
 * Would prefer to avoid this, have bindings just store
 * the two numbers would make it easier to deal with but
 * not as readable in the XML.
 */
void OldBindingEditor::captureForm(Binding* b, bool includeTarget)
{
    // item 0 is global, then tracks, then groups
    int item = scope.getSelection();
    if (item == 0) {
        // global
        b->scope = "";
    }
    else if (item <= maxTracks) {
        // track number
        b->scope = juce::String(item);
    }
    else {
        // skip going back to the SystemConfig for the names and
        // just remove our prefix
        juce::String itemName = scope.getSelectionText();
        juce::String groupName = itemName.fromFirstOccurrenceOf("Group ", false, false);
        b->scope = groupName;
    }

    captureSubclassFields(b);
    
    b->arguments = arguments.getValue();
    b->release = release.getValue();

    // if we're doing immediate captures of the form without Update
    // this should be false so the target remains in place
    // if we're using the Update button, this would be true
    if (includeTarget)
      targets.capture(b);
}

/**
 * Should be called whenever a change is detected to something in the form.
 * This includes fields managed here, and in the subclass.
 * Subclass is responsible for intercepting changes and calling this.
 *
 * This is an alternative to requiring the use of the Uppdate button
 * to capture the form fields into the current binding.
 * Assuming this is working properly, we don't need the Update button any more.
 *
 * We could try to be more granular and only update the field that changed
 * but there aren't many of them, and it's a pita.
 */
void OldBindingEditor::formChanged()
{
    Binding* current = bindings.getSelectedBinding();
    if (current != nullptr) {
        // target shouldn't have changed, but ask to exclude it anyway
        captureForm(current, false);
        bindings.updateContent();
    }
}

/**
 * Should be called whenever a change is detected in the binding target
 * subcomponent.  Like formChanged, this is expected to update the
 * current binding if there is one.
 */
void OldBindingEditor::targetChanged()
{
    Binding* current = bindings.getSelectedBinding();
    if (current != nullptr) {
        targets.capture(current);
        bindings.updateContent();
    }
}

//////////////////////////////////////////////////////////////////////
//
// BindingTable Listener
//
//////////////////////////////////////////////////////////////////////

/**
 * Render the cell that represents the binding trigger.
 */
juce::String OldBindingEditor::renderTriggerCell(Binding* b)
{
    // subclass must overload this
    return renderSubclassTrigger(b);
}

/**
 * Update the binding info components to show things for the
 * binding selected in the table
 */
void OldBindingEditor::bindingSelected(Binding* b)
{
    if (bindings.isNew(b)) {
        // uninitialized row, don't modify it but reset the target display
        resetFormAndTarget();
    }
    else {
        refreshForm(b);
    }
    
}

void OldBindingEditor::bindingDeselected()
{
    resetFormAndTarget();
}

/**
 * The "New" button is clicked.
 * Two options here:
 *   1) Create an empty row and requires an Update click after filling out the form
 *   2) Create a new row filled with the current content of the form
 *
 * 1 is how I started, but Mobius 2 did option 2 which everyone expects
 * and is easier since you don't have to remember to click Update.
 */
Binding* OldBindingEditor::bindingNew()
{
    Binding* neu = nullptr;
    
    // what everyone expects
    bool captureCurrentTarget = true;

    if (captureCurrentTarget && targets.isTargetSelected()) {
        neu = new Binding();
        captureForm(neu, true);
    }
    else {
        // we'l let BindingTable make a placeholder row
        // clear any lingering target selection?
        resetFormAndTarget();
    }

    return neu;
}

/**
 * The Copy/Duplicate button is clicked.
 * This is like bindingNew except we don't capture the form, it makes
 * a copy of the selected binding which is passed.
 */
Binding* OldBindingEditor::bindingCopy(Binding* src)
{
    Binding* neu = new Binding(src);
    // since this is identical to the other one, don't need to refresh
    // the form and target
    return neu;
}

void OldBindingEditor::bindingUpdate(Binding* b)
{
    // was ignoring this if !target.isTargetSelected
    // but I suppose we can go ahead and capture what we have
    captureForm(b, true);
}

void OldBindingEditor::bindingDelete(Binding* b)
{
    (void)b;
    resetFormAndTarget();
}

/**
 * BindingTableSelector Listener called when the user manually
 * clicks on one of the targets.
 *
 * Originally this deselected everything and initialized the form,
 * but that isn't consistent with the way the form now works by
 * auto updating the selected binding.
 *
 * Old thoughts said that it could try to locate a binding
 * with that target and select it, but I'm not liking that.
 */
void OldBindingEditor::bindingTargetClicked()
{
    targetChanged();

    // old way before form capture
    //resetForm();
    //bindings.deselect();
}

//////////////////////////////////////////////////////////////////////
//
// Component
//
//////////////////////////////////////////////////////////////////////

void OldBindingEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    // leave a little gap on the left
    area.removeFromLeft(10);
    
    // leave some space at the top for the overlay checkbox
    activationButtons.setBounds(area.removeFromTop(20));

    // let's fix the size of the table for now rather
    // adapt to our size
    int width = bindings.getPreferredWidth();
    int height = bindings.getPreferredHeight();

    // give the targets more room
    width -= 50;
    bindings.setBounds(area.getX(), area.getY(), width, height);
    
    area.removeFromLeft(bindings.getWidth() + 10);
    // need enough room for arguments so shorten it
    // could try to adapt to the size of the argumnts Form instead
    
    targets.setBounds(area.getX(), area.getY(), 400, 300);

    //form->render();
    form.setBounds(area.getX(), targets.getY() + targets.getHeight() + 8,
                   400, form.getPreferredHeight());
    //form.setTopLeftPosition(area.getX(), targets.getY() + targets.getHeight() + 10);
}    

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
