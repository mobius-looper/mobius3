
#include <JuceHeader.h>

#include <string>
#include <sstream>

#include "../../util/Trace.h"
#include "../../model/MobiusConfig.h"
#include "../../model/UIConfig.h"
#include "../../model/Binding.h"

#include "ConfigEditor.h"
#include "BindingPanel.h"
#include "ButtonPanel.h"

ButtonPanel::ButtonPanel(ConfigEditor* argEditor) :
    BindingPanel(argEditor, "Button Sets", true)
{
    setName("ButtonPanel");

    // we don't need a trigger column
    // sadly the BindingTable has already been constructed at this point
    // and we didn't have a way to suppress this up front
    bindings.removeTrigger();

    // add the column to show the display name
    bindings.addDisplayName();

    // show the up/down buttons for ordering until we can have drag and drop
    bindings.setOrdered(true);
    
    // now that BindingPanel is fully constructed
    // initialize the form so it can call down to our virtuals
    initForm();
}

ButtonPanel::~ButtonPanel()
{
}

/**
 * Want to reuse the same BindingPanel and BindingTable but
 * we're not dealing with the Binding model now in UIConfig.
 * Overload the load and save methods.
 */
void ButtonPanel::load()
{
    if (!loaded) {
        MobiusConfig* mconfig = editor->getMobiusConfig();
        UIConfig* config = editor->getUIConfig();

        // BingingPanel::load normally does this but since we
        // overload load() we have to do it
        maxTracks = mconfig->getTracks();
        maxGroups = mconfig->getTrackGroups();
        targets.load();
        resetForm();

        // make a local copy of the ButtonSets and build
        // the name array for ObjectSelector
        buttons.clear();
        revertButtons.clear();
        juce::Array<juce::String> names;
        for (auto set : config->buttonSets) {
            ButtonSet* copy = new ButtonSet(set);
            if (copy->name.length() == 0)
              copy->name = "[No Name]";
            names.add(copy->name);
            buttons.add(copy);
            revertButtons.add(new ButtonSet(set));
        }
        // this will also auto-select the first one
        objectSelector.setObjectNames(names);

        // todo: really need to find a way to deal with "named object lists"
        // in a generic way with OwnedArray, c++ makes this too fucking hard
        // maybe some sort of transient container Map that also gets rid
        // of linear name searches
        selectedButtons = 0;
        juce::String active = config->activeButtonSet;
        if (active.length() > 0) {
            int index = 0;
            for (auto set : config->buttonSets) {
                if (set->name == active) {
                    selectedButtons = index;
                    break;
                }
                index++;
            }
        }
        objectSelector.setSelectedObject(selectedButtons);
        
        loadButtons(selectedButtons);

        // force this true for testing
        changed = true;
        loaded = true;
    }
}

/**
 * Convert BindingTable/Binding back into DisplayButtons.
 */
void ButtonPanel::save()
{
    if (changed) {

        // capture the final editing state for the selected set
        saveButtons(selectedButtons);

        // build a new ButtonSet list and leave it in the
        // master config
        UIConfig* config = editor->getUIConfig();

        config->activeButtonSet = (buttons[selectedButtons])->name;
        config->buttonSets.clear();
        
        while (buttons.size() > 0) {
            ButtonSet* neu = buttons.removeAndReturn(0);
            config->buttonSets.add(neu);
        }

        editor->saveUIConfig();

        changed = false;
        loaded = false;
    }
    else if (loaded) {
        cancel();
    }
}

void ButtonPanel::cancel()
{
    buttons.clear();
    revertButtons.clear();
    loaded = false;
    changed = false;
}

//////////////////////////////////////////////////////////////////////
//
// BindingPanel/ButtonSet Conversion
//
//////////////////////////////////////////////////////////////////////

/**
 * Load one of the ButtonSets into the BindingPanel UI
 *
 * This does a model conversion from the DisplayButton to
 * a Binding.  Assign a transient id to each so we can
 * correlate them on save.
 */
void ButtonPanel::loadButtons(int index)
{
    bindings.clear();
    ButtonSet* set = buttons[index];
    int id = 0;
    for (auto button : set->buttons) {
        button->id = id;
        // pretend it is a Binding for BindingTable
        // shouldn't have an empty string but filter if we do
        if (button->action.length() > 0) {
            Binding b;
            b.id = id;
            b.setSymbolName(button->action.toUTF8());

            // Binding wants "global" scope represented
            // as a nullptr, not an empty string
            if (button->scope.length() > 0)
              b.setScope(button->scope.toUTF8());
                
            if (button->arguments.length() > 0)
              b.setArguments(button->arguments.toUTF8());
            // table will copy
            bindings.add(&b);
        }
        id++;
    }
    bindings.updateContent();
}

/**
 * Save the state of the BindingTable into a ButtonSet.
 * This is used to capture edits made when switching sets,
 * or on the final save.
 *
 * This is awkward since the Binding model we're editing isn't
 * the same as the DisplayButton we're saving.  To properly detect
 * deletion, we need to match them, but just the action isn't enough
 * because there could be several with different arguments.  If we
 * just recreate the entire list like the other binding panels we'll
 * lose information in the DisplayButton that isn't in the Binding
 * like the display name.  Also really need to support reordering.
 * I gave both Binding and DisplayButton a transient "id" variable
 * so we can correlate them, but really need to have a completely
 * different ButtonTable so we don't have to deal with this.  
 */
void ButtonPanel::saveButtons(int index)
{
    ButtonSet* set = buttons[index];

    // start building a new DisplayButton list
    juce::Array<DisplayButton*> newButtons;
    
    // we own this list now
    Binding* bindingList = bindings.captureBindings();
    Binding* binding = bindingList;
    while (binding != nullptr) {

        // find the corresponding DisplayButton
        DisplayButton* match = nullptr;
        for (auto b : set->buttons) {
            if (b->id == binding->id) {
                match = b;
                break;
            }
        }

        if (match == nullptr) {
            // must be a new one
            match = new DisplayButton();
        }
        else {
            // move the currently owned button to the new list
            set->buttons.removeObject(match, false);
        }
        newButtons.add(match);
        
        match->action = binding->getSymbolName();
        match->arguments = binding->getArguments();
        match->scope = binding->getScope();

        binding = binding->getNext();
    }
    // delete the captured list
    delete bindingList;

    // at this point, newButtons has the ones we want to keep
    // and what remains in set->buttons were deleted
    set->buttons.clear();
    for (auto button : newButtons)
      set->buttons.add(button);
}

//////////////////////////////////////////////////////////////////////
//
// ObjectSelector Overloads
//
//////////////////////////////////////////////////////////////////////

/**
 * Called when the combobox changes.
 */
void ButtonPanel::selectObject(int ordinal)
{
    if (ordinal != selectedButtons) {
        saveButtons(selectedButtons);
        selectedButtons = ordinal;
        loadButtons(selectedButtons);
    }
}

void ButtonPanel::newObject()
{
    int newOrdinal = buttons.size();
    ButtonSet* neu = new ButtonSet();
    neu->name = "[New]";

    // Complex config editors like PresetPanel copy the current
    // object into the new one.  For ButtonSet I think it makes more
    // sense to start over with an empty set?

    buttons.add(neu);
    // make another copy for revert
    revertButtons.add(new ButtonSet(neu));
    
    objectSelector.addObjectName(neu->name);
    // select the one we just added
    objectSelector.setSelectedObject(newOrdinal);
    selectedButtons = newOrdinal;
    loadButtons(selectedButtons);
}

/**
 * Delete is somewhat complicated.
 * You can't undo it unless we save it somewhere.
 * An alert would be nice, ConfigPanel could do that.
 */
void ButtonPanel:: deleteObject()
{
    if (buttons.size() == 1) {
        // Unlike Presets which must have at least one, we don't
        // need any ButtonSets, though most users have one.
        // If you want to hide it, can use another UIConfig option
        // to show/hide the button area
    }
    else {
        buttons.remove(selectedButtons);
        revertButtons.remove(selectedButtons);
        // leave the index where it was and show the next one,
        // if we were at the end, move back
        int newOrdinal = selectedButtons;
        if (newOrdinal >= buttons.size())
          newOrdinal = buttons.size() - 1;
        selectedButtons = newOrdinal;
        loadButtons(selectedButtons);
    }
}

void ButtonPanel::revertObject()
{
    ButtonSet* reverted = new ButtonSet(revertButtons[selectedButtons]);
    buttons.set(selectedButtons, reverted);
    // what about the ObjectSelector name!!
    loadButtons(selectedButtons);
}

/**
 * Called when the ObjectSelector's ComboBox changed the name.
 */
void ButtonPanel::renameObject(juce::String newName)
{
    ButtonSet* set = buttons[selectedButtons];
    set->name = objectSelector.getObjectName();
}

//////////////////////////////////////////////////////////////////////
//
// BindingPanel Overloads
//
// Mostly not relevant except for the subclass fields which is where
// we show and edit the alternate display name.
// The dual-model is annoying, BindingTable uses the old Binding which
// doesn't have a displayName, so have to match it with the DisplayButton
// to get/set the name.
//
//////////////////////////////////////////////////////////////////////

bool ButtonPanel::isRelevant(Binding* b)
{
    (void)b;
    return true;
}

/**
 * Return the string to show in the trigger column for a binding.
 * The trigger column should be suppressed for buttons so we won't get here
 */
juce::String ButtonPanel::renderSubclassTrigger(Binding* b)
{
    (void)b;
    return juce::String();
}

void ButtonPanel::addSubclassFields()
{
    displayName = new Field("Display Name", Field::Type::String);
    displayName->setWidthUnits(20);
    form.add(displayName);
}

/**
 * Locate the DisplayButton that corresponds to this Binding in the table
 * The binding will have the true target name.
 */
DisplayButton* ButtonPanel::getDisplayButton(Binding* binding)
{
    DisplayButton* button = nullptr;
    ButtonSet* set = buttons[selectedButtons];
    for (auto b : set->buttons) {
        if (b->action == binding->getSymbolName()) {
            button = b;
            break;
        }
    }
    return button;
}
            
void ButtonPanel::refreshSubclassFields(Binding* b)
{
    DisplayButton* button = getDisplayButton(b);
    if (button != nullptr) 
      displayName->setValue(button->name);
}

void ButtonPanel::captureSubclassFields(class Binding* b)
{
    // not necessary, but continue with this in case something
    // needs a Trigger
    b->trigger = TriggerUI;

    juce::String dname = displayName->getValue().toString();
    DisplayButton* button = getDisplayButton(b);
    if (button != nullptr)
      button->name = dname;
}

void ButtonPanel::resetSubclassFields()
{
    displayName->setValue("");
}

/**
 * Unusual overload just for buttons since the other triggers
 * aren't visible.
 */
juce::String ButtonPanel::getDisplayName(Binding* b)
{
    juce::String buttonDisplayName;
    // can be nullptr if this is new
    DisplayButton* button = getDisplayButton(b);
    if (button != nullptr)
      buttonDisplayName = button->name;
    return buttonDisplayName;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
