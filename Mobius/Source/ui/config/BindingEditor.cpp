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
 * Under the BindingTargetPanel are Extended Fields to add additional information about
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
#include "../../model/MobiusConfig.h"
#include "../../model/Binding.h"
#include "../common/Form.h"
#include "../JuceUtil.h"

#include "BindingTable.h"
#include "BindingTargetPanel.h"

#include "BindingEditor.h"

BindingEditor::BindingEditor()
{
    setName("BindingEditor");
    
    // this one is selectively shown
    addChildComponent(activationButtons);
    
    activeButton.setColour(juce::ToggleButton::ColourIds::textColourId, juce::Colours::white);
    activeButton.setColour(juce::ToggleButton::ColourIds::tickColourId, juce::Colours::red);
    activeButton.setColour(juce::ToggleButton::ColourIds::tickDisabledColourId, juce::Colours::white);
    activationButtons.add(&activeButton);
    mergeButton.setColour(juce::ToggleButton::ColourIds::textColourId, juce::Colours::white);
    mergeButton.setColour(juce::ToggleButton::ColourIds::tickColourId, juce::Colours::red);
    mergeButton.setColour(juce::ToggleButton::ColourIds::tickDisabledColourId, juce::Colours::white);
    activationButtons.add(&mergeButton);
    
    bindings.setListener(this);
    addAndMakeVisible(bindings);
    addAndMakeVisible(targets);

    addAndMakeVisible(form);
}

BindingEditor::~BindingEditor()
{
}

/**
 * ConfigPanel overload to prepare the panel to be shown.
 * Make copies of all the BindingSets in bindingSets and revertBindingSets.
 * Load the first BindingSet into the BindingTable.
 */
void BindingEditor::load()
{
    MobiusConfig* config = context->getMobiusConfig();

    maxTracks = config->getTracks();
    maxGroups = config->getTrackGroups();

    targets.load();

    // Though only MidiPanel supports overlays, handle all three
    // the same.  ButtonPanel overloads this differently

    bindingSets.clear();
    revertBindingSets.clear();

    BindingSet* setlist = config->getBindingSets();
    if (setlist == nullptr) {
        // must be a misconfigured install, shouldn't happen
        setlist = new BindingSet();
        setlist->setName("Base");
        config->setBindingSets(setlist);
    }

    while (setlist != nullptr) {
        BindingSet* set = new BindingSet(setlist);

        // first set doesn't always have a name, force one
        if (bindingSets.size() == 0 && set->getName() == nullptr)
          set->setName("Base");
                
        bindingSets.add(set);
        revertBindingSets.add(new BindingSet(set));

        setlist = setlist->getNextBindingSet();
    }
        
    selectedBindingSet = 0;
    loadBindingSet(selectedBindingSet);

    refreshObjectSelector();
        
    resetForm();
}

/**
 * Refresh the object selector on initial load and after any
 * objects are added or removed.  This could be pushed up to
 * ConfigPanel if each subclass had a method to return
 * the list of names and the current selection, but at that point
 * you're not eliminating much duplication.
 */
void BindingEditor::refreshObjectSelector()
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

void BindingEditor::loadBindingSet(int index)
{
    bindings.clear();
    BindingSet* set = bindingSets[index];
    if (set != nullptr) {
        Binding* blist = set->getBindings();
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
    resetForm();

    // this is shown only when editing one of the overlay sets
    // hmm, think about adding an "active" here too, if you're editing
    activationButtons.setVisible(index > 0);
    activeButton.setToggleState(set->isActive(), juce::NotificationType::dontSendNotification);
    mergeButton.setToggleState(set->isMerge(), juce::NotificationType::dontSendNotification);
}

/**
 * Called by the Save button in the footer.
 * 
 * Save all presets that have been edited during this session
 * back to the master configuration.
 */
void BindingEditor::save()
{
    // capture visible state in the table back into
    // the current BidningSet
    saveBindingSet(selectedBindingSet);
        
    // build a new BindingSet linked list
    BindingSet* setlist = nullptr;
    BindingSet* last = nullptr;
        
    for (int i = 0 ; i < bindingSets.size() ; i++) {
        BindingSet* set = bindingSets[i];
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

    MobiusConfig* config = context->getMobiusConfig();
    // this also deletes the current list
    config->setBindingSets(setlist);
    context->saveMobiusConfig();
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
void BindingEditor::saveBindingSet(int index)
{
    BindingSet* set = bindingSets[index];
    if (set != nullptr) {
        saveBindingSet(set);

        if (index > 0) {
            set->setActive(activeButton.getToggleState());
            set->setMerge(mergeButton.getToggleState());
            // if this is an exclusive overlay, turn off the
            // activation state of the others so the selection menus look right
            // when we're done
            if (set->isActive()) {
                for (int i = 1 ; i < bindingSets.size() ; i++) {
                    if (i != index) {
                        BindingSet* other = bindingSets[i];
                        if (!set->isMerge() && !other->isMerge())
                          other->setActive(false);
                    }
                }
            }
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
void BindingEditor::saveBindingSet(BindingSet* dest)
{
    // note well: unlike most object lists, MobiusConfig::setBindingSets does
    // NOT delete the current Binding list, it just takes the pointer
    // so we can reconstruct the list and set it back without worrying
    // about dual ownership.  Deleting a Binding DOES however follow the
    // chain so be careful with that.  Really need model cleanup.
    juce::Array<Binding*> newBindings;

    Binding* original = dest->getBindings();
    dest->setBindings(nullptr);
        
    while (original != nullptr) {
        // take it out of the list to prevent cascaded delete
        Binding* next = original->getNext();
        original->setNext(nullptr);
        if (!isRelevant(original))
          newBindings.add(original);
        else
          delete original;
        original = next;
    }
        
    // now add back the edited ones, some may have been deleted
    // and some may be new
    Binding* edited = bindings.captureBindings();
    while (edited != nullptr) {
        Binding* next = edited->getNext();
        edited->setNext(nullptr);
        newBindings.add(edited);
        edited = next;
    }

    // link them back up
    Binding* merged = nullptr;
    Binding* last = nullptr;
    for (int i = 0 ; i < newBindings.size() ; i++) {
        Binding* b = newBindings[i];
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
void BindingEditor::cancel()
{
    // throw away the copies
    Binding* blist = bindings.captureBindings();
    delete blist;
    
    // delete the copied sets
    bindingSets.clear();
    revertBindingSets.clear();
}

void BindingEditor::revert()
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
void BindingEditor::objectSelectorSelect(int ordinal)
{
    if (ordinal != selectedBindingSet) {
        saveBindingSet(selectedBindingSet);
        selectedBindingSet = ordinal;
        loadBindingSet(selectedBindingSet);
    }
}

void BindingEditor::objectSelectorNew(juce::String name)
{
    int newOrdinal = bindingSets.size();
    BindingSet* neu = new BindingSet();
    neu->setName("[New]");

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
void BindingEditor::objectSelectorDelete()
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

void BindingEditor::objectSelectorRename(juce::String newName)
{
    BindingSet* set = bindingSets[selectedBindingSet];
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
void BindingEditor::initForm()
{
    // scope always goes first
    juce::StringArray scopeNames;
    scopeNames.add("Global");

    if (maxTracks == 0) maxTracks = 8;
    for (int i = 0 ; i < maxTracks ; i++)
      scopeNames.add("Track " + juce::String(i+1));

    if (maxGroups == 0) maxGroups = 2;
    for (int i = 0 ; i < maxGroups ; i++) {
        // passing a char to the String constructor didn't work,
        // it rendered the numeric value of the character
        // operator += works
        // juce::String((char)('A' + i)));
        juce::String letter;
        letter += (char)('A' + i);
        scopeNames.add("Group " + letter);
    }
    
    scope = new Field("Scope", Field::Type::String);
    scope->setAllowedValues(scopeNames);
    form.add(scope);

    // subclass gets to add it's fields
    addSubclassFields();

    // arguments last
    arguments = new Field("Arguments", Field::Type::String);
    arguments->setWidthUnits(20);
    form.add(arguments);
    form.render();
}

/**
 * Reset all trigger and target arguments to their initial state
 */
void BindingEditor::resetForm()
{
    scope->setValue(juce::var(0));
    
    targets.reset();

    resetSubclassFields();
    
    arguments->setValue(juce::var());
}

/**
 * Refresh form to have values for the selected binding
 *
 * Binding model represents scopes as a string, then parses
 * that into track or group numbers.  
 */
void BindingEditor::refreshForm(Binding* b)
{
    const char* scopeName = b->getScope();
    if (scopeName == nullptr) {
        scope->setValue(juce::var(0));
    }
    else {
        // can assume these have been set as a side effect
        // of calling setScope(char), don't like this
        // track and group numbers are 1 based if they are set
        int tracknum = b->trackNumber;
        if (tracknum > 0) {
            // element 0 is "global" so track number works
            scope->setValue(juce::var(tracknum));
        }
        else {
            int groupnum = b->groupOrdinal;
            if (groupnum > 0)
              scope->setValue(juce::var(maxTracks + groupnum));
        }
    }
    
    targets.select(b);
    refreshSubclassFields(b);
    
    juce::var args = juce::var(b->getArguments());
    arguments->setValue(args);
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
void BindingEditor::captureForm(Binding* b)
{
    // item 0 global, tracks, groups
    int item = scope->getIntValue();
    if (item == 0) {
        // global, this ensures both track and group are cleared
        b->setScope(nullptr);
    }
    else if (item <= maxTracks) {
        // this will format the track letter
        b->setTrack(item);
    }
    else {
        // the group number in the Binding starts with 1
        b->setGroup(item - maxTracks);
    }

    targets.capture(b);

    // todo: scope
    captureSubclassFields(b);
    
    juce::var value = arguments->getValue();
    b->setArguments(value.toString().toUTF8());
}

//////////////////////////////////////////////////////////////////////
//
// BindingTable Listener
//
//////////////////////////////////////////////////////////////////////

/**
 * Render the cell that represents the binding trigger.
 */
juce::String BindingEditor::renderTriggerCell(Binding* b)
{
    // subclass must overload this
    return renderSubclassTrigger(b);
}

/**
 * Update the binding info components to show things for the
 * binding selected in the table
 */
void BindingEditor::bindingSelected(Binding* b)
{
    if (bindings.isNew(b)) {
        // uninitialized row, don't modify it but reset the target display
        resetForm();
    }
    else {
        refreshForm(b);
    }
    
}

Binding* BindingEditor::bindingNew()
{
    Binding* neu = nullptr;
    
    // here we have the option of doing an immediate capture
    // of what is in the target selectors, return null
    // to leave a [New] placeholder row
    bool captureCurrentTarget = false;

    if (captureCurrentTarget && targets.isTargetSelected()) {
        neu = new Binding();
        captureForm(neu);
    }
    else {
        // we'l let BindingTable make a placeholder row
        // clear any lingering target selection?
        resetForm();
    }

    return neu;
}

void BindingEditor::bindingUpdate(Binding* b)
{
    // was ignoring this if !target.isTargetSelected
    // but I suppose we can go ahead and capture what we have
    captureForm(b);
}

void BindingEditor::bindingDelete(Binding* b)
{
    (void)b;
    resetForm();
}

/**
 * Field::Listener callback
 * If there is something selected in the table could actively
 * change it, but we're going with a manual Update button for now
 * now that we have a Form this would have to be a lot more
 * complicated
 */
void BindingEditor::fieldChanged(Field* field)
{
    (void)field;
}

//////////////////////////////////////////////////////////////////////
//
// Component
//
//////////////////////////////////////////////////////////////////////

void BindingEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    // leave a little gap on the left
    area.removeFromLeft(10);
    
    // leave some space at the top for the merge checkbox
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
    form.setTopLeftPosition(area.getX(), targets.getY() + targets.getHeight() + 10);
}    

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
