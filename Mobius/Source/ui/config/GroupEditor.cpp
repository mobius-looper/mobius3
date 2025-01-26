/**
 * A ConfigEditor for editing GroupDefinitions.
 */

#include <JuceHeader.h>

#include "../../model/MobiusConfig.h"
#include "../../model/GroupDefinition.h"
#include "../../model/Symbol.h"
#include "../../model/FunctionProperties.h"
#include "../../model/ParameterProperties.h"
#include "../../Supervisor.h"

#include "../common/YanForm.h"
#include "../common/YanField.h"
#include "../JuceUtil.h"

#include "MultiSelectDrag.h"

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
    //form.setLabelCharWidth(10);
    form.setTopInset(12);
    form.add(&color);
    form.add(&replication);
    addAndMakeVisible(form);

    color.setListener(this);

    juce::StringArray allowed;
    for (auto symbol : supervisor->getSymbols()->getSymbols()) {
        if (symbol->functionProperties != nullptr &&
            symbol->functionProperties->mayFocus)
          allowed.add(symbol->name);
    }

    functions.setAllowed(allowed);
    //functions.setLabel("Focus Lock Functions");
    tabs.add("Focus Lock Functions", &functions);

    allowed.clear();
    for (auto symbol : supervisor->getSymbols()->getSymbols()) {
        if (symbol->parameterProperties != nullptr &&
            symbol->parameterProperties->mayFocus)
          allowed.add(symbol->name);
    }
    parameters.setAllowed(allowed);
    tabs.add("Focus Lock Parameters", &parameters);
    
    addAndMakeVisible(tabs);
}

void GroupEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    form.setBounds(area.removeFromTop(form.getPreferredHeight()));
    area.removeFromTop(20);
    tabs.setBounds(area.removeFromTop(200));
}

/**
 * Load all the GroupDefinitions, nice and fresh.
 */
void GroupEditor::load()
{
    juce::Array<juce::String> names;
    groups.clear();
    revertGroups.clear();
    MobiusConfig* config = supervisor->getOldMobiusConfig();
    for (auto src : config->groups) {
        // Supervisor should have upgraded these by now
        if (src->name.length() == 0) {
            Trace(1, "GroupEditor: GroupDefinition with no name, bad Supervisor");
        }
        else {
            groups.add(new GroupDefinition(src));
            revertGroups.add(new GroupDefinition(src));
            names.add(src->name);
        }
    }
    
    // this will also auto-select the first one
    context->setObjectNames(names);

    // load the first one, do we need to bootstrap one if
    // we had an empty config?
    selectedGroup = 0;
    loadGroup(selectedGroup);
}

/**
 * Refresh the object selector on initial load and after any
 * objects are added or removed.
 */
void GroupEditor::refreshObjectSelector()
{
    juce::Array<juce::String> names;
    for (auto group : groups)
      names.add(group->name);
    context->setObjectNames(names);
    context->setSelectedObject(selectedGroup);
}

/**
 * Called by the Save button in footer.
 * 
 * Save all presets that have been edited during this session
 * back to the master configuration.
 *
 * !!TODO: Group names can be in the Session and Bindings and if you
 * rename them, the user will expect that those references are updated automatically.
 * It's a little complex if you add/remove objects and the old count and the new count
 * don't match and the names don't line up any more.  Will have to give each starting
 * object a unique id we can use to check for name changes.
 */
void GroupEditor::save()
{
    // copy visible state back into the GroupDefinition
    // need to also do this when the selected group is changed
    saveGroup(selectedGroup);

    juce::Array<GroupDefinition*> newList;
    while (groups.size() > 0)
      newList.add(groups.removeAndReturn(0));

    supervisor->groupEditorSave(newList);
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

    // names have historically been generated with a letter
    // and the ObjectSelector won't pass in a new name anyway
    neu->name = GroupDefinition::getInternalName(newOrdinal);

    groups.add(neu);
    // make another copy for revert
    GroupDefinition* revert = new GroupDefinition(neu);
    revertGroups.add(revert);

    selectedGroup = newOrdinal;
    loadGroup(selectedGroup);

    context->addObjectName(neu->name);
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
        // name has already been handled by the object selector
        // bootsrap the color if not set
        int startcolor = g->color;
        if (startcolor == 0)
          startcolor = juce::Colours::white.getARGB();
        color.setValue(startcolor);

        replication.setValue(g->replicationEnabled);
        functions.setValue(g->replicatedFunctions);
        parameters.setValue(g->replicatedParameters);
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

        g->color = color.getValue();
        g->replicationEnabled = replication.getValue();
        g->replicatedFunctions = functions.getValue();
        g->replicatedParameters = parameters.getValue();
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
// Colors
//
//////////////////////////////////////////////////////////////////////

void GroupEditor::colorSelected(int argb)
{
    (void)argb;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
