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
#include "../../model/old/MobiusConfig.h"
#include "../../model/old/OldBinding.h"
#include "../../model/Scope.h"
#include "../JuceUtil.h"
#include "../MobiusView.h"

// temporary until we can get the initialization order sorted out
#include "../../Supervisor.h"
#include "../../Grouper.h"

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
    MobiusConfig* config = supervisor->getOldMobiusConfig();
    MobiusView* view = supervisor->getMobiusView();

    maxTracks = view->totalTracks;
    //maxGroups = config->groups.size();

    refreshScopeNames();
    targets.load();

    // Though only MidiPanel supports overlays, handle all three
    // the same.  ButtonPanel overloads this differently

    bindingSets.clear();
    revertBindingSets.clear();

    OldBindingSet* setlist = config->getBindingSets();
    if (setlist == nullptr) {
        // must be a misconfigured install, shouldn't happen
        setlist = new OldBindingSet();
        setlist->setName("Base");
        config->setBindingSets(setlist);
    }

    // copy all the BindingSets in the source
    while (setlist != nullptr) {
        OldBindingSet* set = new OldBindingSet(setlist);

        // first set doesn't always have a name, force one
        if (bindingSets.size() == 0 && set->getName() == nullptr)
          set->setName("Base");
                
        bindingSets.add(set);
        revertBindingSets.add(new OldBindingSet(set));

        setlist = setlist->getNextBindingSet();
    }

    if (initialObject.length() > 0) {
        // this is the first time here for an editor that supports
        // multiple binding sets, which is really only MidiEditor
        // pre-select this one since it is likely it will be the first
        // one to be edited
        selectedBindingSet = 0;
        for (int i = 0 ; i < bindingSets.size() ; i++) {
            if (juce::String(bindingSets[i]->getName()) == initialObject) {
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
        if (set->getName() == nullptr)
          set->setName("[New]");
        names.add(set->getName());
    }
    context->setObjectNames(names);
    context->setSelectedObject(selectedBindingSet);
}

void OldBindingEditor::loadBindingSet(int index)
{
    bindings.clear();
    OldBindingSet* set = bindingSets[index];
    if (set != nullptr) {
        OldBinding* blist = set->getBindings();
        while (blist != nullptr) {
            // subclass overload
            if (isRelevant(blist)) {
                // table will copy
                bindings.add(blist);
            }
            blist = blist->getNext();
        }
    }
    bindings.updateContent();
    resetFormAndTarget();

    // this is shown only when editing one of the overlay sets
    // hmm, think about adding an "active" here too, if you're editing
    // update: no longer doing activation here, there is only the "overlay" flag
    activationButtons.setVisible(index > 0);
    //activeButton.setToggleState(set->isActive(), juce::NotificationType::dontSendNotification);
    overlayButton.setToggleState(set->isOverlay(), juce::NotificationType::dontSendNotification);
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
        
    // build a new BindingSet linked list
    OldBindingSet* setlist = nullptr;
    OldBindingSet* last = nullptr;
        
    for (int i = 0 ; i < bindingSets.size() ; i++) {
        OldBindingSet* set = bindingSets[i];
        if (last == nullptr)
          setlist = set;
        else
          last->setNext(set);
        last = set;
    }

    // we took ownership of the objects so
    // clear the owned array but don't delete them
    bindingSets.clear(false);
    // these we don't need any more
    revertBindingSets.clear();

    supervisor->bindingEditorSave(setlist);
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
    OldBindingSet* set = bindingSets[index];
    if (set != nullptr) {
        saveBindingSet(set);

        if (index > 0) {
            //set->setActive(activeButton.getToggleState());
            set->setOverlay(overlayButton.getToggleState());
            // if this is an exclusive overlay, turn off the
            // activation state of the others so the selection menus look right
            // when we're done
            // update: activation is no longer done in the editor
#if 0            
            if (set->isActive()) {
                for (int i = 1 ; i < bindingSets.size() ; i++) {
                    if (i != index) {
                        OldBindingSet* other = bindingSets[i];
                        if (!set->isOverlay() && !other->isOverlay())
                          other->setActive(false);
                    }
                }
            }
#endif
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
void OldBindingEditor::saveBindingSet(OldBindingSet* dest)
{
    // note well: unlike most object lists, MobiusConfig::setBindingSets does
    // NOT delete the current Binding list, it just takes the pointer
    // so we can reconstruct the list and set it back without worrying
    // about dual ownership.  Deleting a Binding DOES however follow the
    // chain so be careful with that.  Really need model cleanup.
    juce::Array<OldBinding*> newBindings;

    OldBinding* original = dest->getBindings();
    dest->setBindings(nullptr);
        
    while (original != nullptr) {
        // take it out of the list to prevent cascaded delete
        OldBinding* next = original->getNext();
        original->setNext(nullptr);
        if (!isRelevant(original))
          newBindings.add(original);
        else
          delete original;
        original = next;
    }
        
    // now add back the edited ones, some may have been deleted
    // and some may be new
    OldBinding* edited = bindings.captureBindings();
    while (edited != nullptr) {
        OldBinding* next = edited->getNext();
        edited->setNext(nullptr);
        newBindings.add(edited);
        edited = next;
    }

    // link them back up
    OldBinding* merged = nullptr;
    OldBinding* last = nullptr;
    for (int i = 0 ; i < newBindings.size() ; i++) {
        OldBinding* b = newBindings[i];
        // clear any residual chain
        b->setNext(nullptr);
        if (last == nullptr)
          merged = b;
        else
          last->setNext(b);
        last = b;
    }

    // put the new list back
    dest->setBindings(merged);
}

/**
 * Throw away all editing state.
 */
void OldBindingEditor::cancel()
{
    // throw away the copies
    OldBinding* blist = bindings.captureBindings();
    delete blist;
    
    // delete the copied sets
    bindingSets.clear();
    revertBindingSets.clear();
}

void OldBindingEditor::revert()
{
    OldBindingSet* revert = revertBindingSets[selectedBindingSet];
    if (revert != nullptr) {
        OldBindingSet* reverted = new OldBindingSet(revert);
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
    OldBindingSet* neu = new OldBindingSet();
    neu->setName("[New]");

    bindingSets.add(neu);
    // make another copy for revert
    OldBindingSet* revert = new OldBindingSet(neu);
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
    OldBindingSet* set = bindingSets[selectedBindingSet];
    set->setName(newName.toUTF8());
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
    // to Supervisor to get to MobiusConfig, this sucks work out a more
    // orderly initialization sequence

    MobiusView* view = supervisor->getMobiusView();
    maxTracks = view->totalTracks;
    for (int i = 0 ; i < maxTracks ; i++)
      scopeNames.add("Track " + juce::String(i+1));

    juce::StringArray gnames;
    Grouper* g = supervisor->getGrouper();
    g->getGroupNames(gnames);

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
void OldBindingEditor::refreshForm(OldBinding* b)
{
    // if anything goes wrong parsing the scope string, leave the
    // selection at "Global"
    scope.setSelection(0);
    
    const char* scopeString = b->getScope();
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
        MobiusConfig* config = supervisor->getOldMobiusConfig();
        int groupOrdinal = Scope::parseGroupOrdinal(config, scopeString);
        if (groupOrdinal >= 0)
          scope.setSelection(maxTracks + groupOrdinal);
        else
          Trace(1, "OldBindingEditor: Binding scope with unresolved group name %s", scopeString);
    }
    
    targets.select(b);
    refreshSubclassFields(b);
    
    juce::String args = juce::String(b->getArguments());
    arguments.setValue(args);
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
void OldBindingEditor::captureForm(OldBinding* b, bool includeTarget)
{
    // item 0 is global, then tracks, then groups
    int item = scope.getSelection();
    if (item == 0) {
        // global
        b->setScope(nullptr);
    }
    else if (item <= maxTracks) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", item);
        b->setScope(buf);
    }
    else {
        // skip going back to the MobiusConfig for the names and
        // just remove our prefix
        juce::String itemName = scope.getSelectionText();
        juce::String groupName = itemName.fromFirstOccurrenceOf("Group ", false, false);
        b->setScope(groupName.toUTF8());
    }

    captureSubclassFields(b);
    
    juce::String value = arguments.getValue();
    b->setArguments(value.toUTF8());
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
    OldBinding* current = bindings.getSelectedBinding();
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
    OldBinding* current = bindings.getSelectedBinding();
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
juce::String OldBindingEditor::renderTriggerCell(OldBinding* b)
{
    // subclass must overload this
    return renderSubclassTrigger(b);
}

/**
 * Update the binding info components to show things for the
 * binding selected in the table
 */
void OldBindingEditor::bindingSelected(OldBinding* b)
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
OldBinding* OldBindingEditor::bindingNew()
{
    OldBinding* neu = nullptr;
    
    // what everyone expects
    bool captureCurrentTarget = true;

    if (captureCurrentTarget && targets.isTargetSelected()) {
        neu = new OldBinding();
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
OldBinding* OldBindingEditor::bindingCopy(OldBinding* src)
{
    OldBinding* neu = new OldBinding(src);
    // since this is identical to the other one, don't need to refresh
    // the form and target
    return neu;
}

void OldBindingEditor::bindingUpdate(OldBinding* b)
{
    // was ignoring this if !target.isTargetSelected
    // but I suppose we can go ahead and capture what we have
    captureForm(b, true);
}

void OldBindingEditor::bindingDelete(OldBinding* b)
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
