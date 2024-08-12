/**
 * A ConfigEditor for editing GroupDefinitions.
 */

#include <JuceHeader.h>

#include "../../model/MobiusConfig.h"
#include "../../model/GroupDefinition.h"
#include "../../model/Symbol.h"
#include "../../model/FunctionProperties.h"
#include "../../Supervisor.h"

#include "../common/Form.h"
#include "../JuceUtil.h"

#include "GroupEditor.h"

GroupEditor::GroupEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("GroupEditor");
}

GroupEditor::~GroupEditor()
{
}

void GroupEditor::prepare()
{
    context->enableObjectSelector();
    context->enableHelp(40);
    context->enableRevert();

    form.setLabelColor(juce::Colours::orange);
    form.setLabelCharWidth(10);
    form.setTopInset(12);
    form.add(&color);
    form.add(&replication);
    addAndMakeVisible(form);

    juce::StringArray allowed;
    for (auto symbol : supervisor->getSymbols()->getSymbols()) {
        if (symbol->functionProperties != nullptr &&
            symbol->functionProperties->mayFocus)
          allowed.add(symbol->name);
    }

    juce::StringArray values;
    functions.setValue(values, allowed);
    functions.setLabel("Replicated Functions");

    addAndMakeVisible(functions);
}

void GroupEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    form.setBounds(area.removeFromTop(100));
    functions.setBounds(area.removeFromTop(200));
}

/**
 * Load all the GroupDefinitions, nice and fresh.
 */
void GroupEditor::load()
{
    // build a list of names for the object selector
    juce::Array<juce::String> names;
    // clone the GroupDefinition list into a local copy
    groups.clear();
    revertGroups.clear();
    MobiusConfig* config = supervisor->getMobiusConfig();
    if (config != nullptr) {
        int ordinal = 0;
        for (auto src : config->groups) {
            groups.add(new GroupDefinition(src));
            revertGroups.add(new GroupDefinition(src));

            if (src->name.length() > 0)
              names.add(src->name);
            else {
                // name them with the canonical letter
                names.add(getInternalName(ordinal));
            }
            ordinal++;
        }

        // upgrade hack
        int oldGroupCount = config->getTrackGroups();
        // make sure we have at least 1, should have been handled already
        if (oldGroupCount == 0)
          oldGroupCount = 2;

        // synthesize GroupDefinitions for the old group count
        if (oldGroupCount > config->groups.size()) {
            ordinal = config->groups.size();
            while (ordinal < oldGroupCount) {
                groups.add(new GroupDefinition());
                revertGroups.add(new GroupDefinition());
                names.add(getInternalName(ordinal));
                ordinal++;
            }
        }
    }
        
    // this will also auto-select the first one
    context->setObjectNames(names);

    // load the first one, do we need to bootstrap one if
    // we had an empty config?
    selectedGroup = 0;
    loadGroup(selectedGroup);
}

juce::String GroupEditor::getInternalName(int ordinal)
{
    // passing a char to the String constructor didn't work,
    // it rendered the numeric value of the character
    // operator += works for some reason
    // juce::String((char)('A' + i)));
    juce::String letter;
    letter += (char)('A' + ordinal);
    return letter;
}

/**
 * Refresh the object selector on initial load and after any
 * objects are added or removed.
 */
void GroupEditor::refreshObjectSelector()
{
    juce::Array<juce::String> names;
    int ordinal = 0;
    for (auto group : groups) {
        if (group->name.length() > 0)
          names.add(group->name);
        else {
            names.add(getInternalName(ordinal));
        }
        ordinal++;
    }
    context->setObjectNames(names);
    context->setSelectedObject(selectedGroup);
}

/**
 * Called by the Save button in footer.
 * 
 * Save all presets that have been edited during this session
 * back to the master configuration.
 */
void GroupEditor::save()
{
    // copy visible state back into the GroupDefinition
    // need to also do this when the selected group is changed
    saveGroup(selectedGroup);

    // replace the definition list in MobiusConfig
    // todo: should we have a minimum number here?
    MobiusConfig* config = supervisor->getMobiusConfig();
    config->groups.clear();
    while (groups.size() > 0)
      config->groups.add(groups.removeAndReturn(0));
        
    revertGroups.clear();

    supervisor->updateMobiusConfig();
}

/**
 * Throw away all editing state.
 */
void GroupEditor::cancel()
{
    groups.clear();
    revertGroups.clear();
}

void GroupEditor::revert()
{
    GroupDefinition* revert = revertGroups[selectedGroup];
    if (revert != nullptr) {
        GroupDefinition* reverted = new GroupDefinition(revert);
        groups.set(selectedGroup, reverted);
        // what about the name?
        loadGroup(selectedGroup);
    }
}

//////////////////////////////////////////////////////////////////////
//
// ObjectSelector overloads
// 
//////////////////////////////////////////////////////////////////////

/**
 * Called when the combobox changes.
 */
void GroupEditor::objectSelectorSelect(int ordinal)
{
    if (ordinal != selectedGroup) {
        saveGroup(selectedGroup);
        selectedGroup = ordinal;
        loadGroup(selectedGroup);
    }
}

void GroupEditor::objectSelectorNew(juce::String newName)
{
    int newOrdinal = groups.size();
    GroupDefinition* neu = new GroupDefinition();

    // group names are optional and we'll add the letter prefix to the object selector
    // neu->setName("[New]");

    groups.add(neu);
    // make another copy for revert
    GroupDefinition* revert = new GroupDefinition(neu);
    revertGroups.add(revert);

    selectedGroup = newOrdinal;
    loadGroup(selectedGroup);

    context->addObjectName(getInternalName(newOrdinal));
    context->setSelectedObject(newOrdinal);
}

/**
 * Delete is somewhat complicated.
 * You can't undo it unless we save it somewhere.
 * An alert would be nice, ConfigPanel could do that.
 */
void GroupEditor::objectSelectorDelete()
{
    if (groups.size() == 1) {
        // must have at least one group, default is 2
    }
    else {
        groups.remove(selectedGroup);
        revertGroups.remove(selectedGroup);
        // leave the index where it was and show the next one,
        // if we were at the end, move back
        int newOrdinal = selectedGroup;
        if (newOrdinal >= groups.size())
          newOrdinal = groups.size() - 1;
        selectedGroup = newOrdinal;
        loadGroup(selectedGroup);
        refreshObjectSelector();
    }
}


void GroupEditor::objectSelectorRename(juce::String newName)
{
    GroupDefinition* group = groups[selectedGroup];
    group->name = newName;
}

//////////////////////////////////////////////////////////////////////
//
// Internal Methods
// 
//////////////////////////////////////////////////////////////////////

/**
 * Load a preset into the parameter fields
 */
void GroupEditor::loadGroup(int index)
{
    GroupDefinition* g = groups[index];
    if (g != nullptr) {

        // todo: load the fields
    }
}

/**
 * Save one of the edited groups back to the master config
 * Think...should save/cancel apply to the entire list of groups
 * or only the one currently being edited.  I think it would be confusing
 * to keep an editing transaction over the entire list
 * When a group is selected, it should throw away changes that
 * are in progress to the current group.
 */
void GroupEditor::saveGroup(int index)
{
    GroupDefinition* g = groups[index];
    if (g != nullptr) {

        // todo: gather the form fields
    }
}

// who called this?
GroupDefinition* GroupEditor::getSelectedGroup()
{
    GroupDefinition* g = nullptr;
    if (groups.size() > 0) {
        if (selectedGroup < 0 || selectedGroup >= groups.size()) {
            // shouldn't happen, default back to first
            selectedGroup = 0;
        }
        g = groups[selectedGroup];
    }
    return g;
}

//////////////////////////////////////////////////////////////////////
//
// Form Rendering
//
//////////////////////////////////////////////////////////////////////

void GroupEditor::initForm()
{
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
