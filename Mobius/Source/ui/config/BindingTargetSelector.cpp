/**
 * Sub component for configuration panels that edit bindings
 * of some form.  Here we present all of the available "targets"
 * for the binding.  A target is defined by an interned Symbol
 * and represent things like functions, parameters, and scripts.
 *
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/List.h"

#include "../../model/ActionType.h"
#include "../../model/FunctionDefinition.h"
#include "../../model/UIParameter.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Preset.h"
#include "../../model/Setup.h"
#include "../../model/Binding.h"
#include "../../model/Symbol.h"

#include "BindingTargetSelector.h"

/**
 * Build the tabbed component for selecting targets.
 * Each time it is shown, load() is called to populate the tabs
 * with the active symbols.
 * 
 * Tabs are: Functions, Scripts, Controls, Parameters, Configurations
 *
 * With the introduction of Symbols, we can assume all targets will
 * have a unique (and possibly qualfified) name.
 */
BindingTargetSelector::BindingTargetSelector()
{
    setName("BindingTargetSelector");

    initBox(&functions);
    addTab(juce::String("Functions"), &functions);

    initBox(&scripts);
    addTab(juce::String("Scripts"), &scripts);
    
    initBox(&controls);
    addTab(juce::String("Controls"), &controls);

    initBox(&configurations);
    addTab(juce::String("Configurations"), &configurations);
    
    initBox(&parameters);
    addTab(juce::String("Parameters"), &parameters);
}

void BindingTargetSelector::initBox(SimpleListBox* box)
{
    box->setMultipleSelectionEnabled(false);
    box->addListener(this);
    boxes.add(box);
}

BindingTargetSelector::~BindingTargetSelector()
{
}

/**
 * Rebuild the data model that underlies the ListBox
 * in each tab.
 *
 * Don't need to rebuild functions, controls, and parameters
 * since they're static, but that could change I suppose and
 * this doesn't happen often.
 */
void BindingTargetSelector::load()
{
    functions.clear();
    scripts.clear();
    controls.clear();
    configurations.clear();
    parameters.clear();
    
    for (auto symbol : Symbols.getSymbols()) {
        
        if (symbol->behavior == BehaviorFunction) {
            // only allow bindings to functions that we define,
            // will filter out the few remaining missing fuctions
            // and the hidden core functions
            if ((symbol->function != nullptr || symbol->id > 0)
                && !symbol->hidden) {
                functions.add(symbol->getName());
            }
        }
        else if (symbol->behavior == BehaviorParameter) {
            UIParameter* p = symbol->parameter;
            if (p != nullptr) {
                // divided into two tabs to put the ones used most in a smaller list
                // these may have display names
                // todo: should be copying the display name to the Symbol
                // NO: can't use display names because those end up in the Binding
                // and we can't search for symbols on that
                // either need some sort of display/name mapping here
                // or store the Symbol in the BindingTable
                if (p->control)
                  // controls.add(p->getDisplayableName());
                  controls.add(symbol->getName());
                else
                  // parameters.add(p->getDisplayableName());
                  parameters.add(symbol->getName());
            }
            else {
                // this isn't catching the two newer UI level parameters
                // for ActiveLayouts and ActiveButtons
                // could add those but they're not that important for
                // bindings and I don't want to mess with the UIParameter/ParameterProperties/Symbol
                // shit right now
            }
        }
        else if (symbol->behavior == BehaviorScript) {
            scripts.add(symbol->getName());
        }
        else if (symbol->behavior == BehaviorActivation) {
            configurations.add(symbol->getName());
        }
    }

    // would be convenient if SimpleListBox could have a sorted flag and
    // it sorted as things were added 
    functions.sort();
    scripts.sort();
    controls.sort();
    configurations.sort();
    parameters.sort();
}

//////////////////////////////////////////////////////////////////////
// Runtime
//////////////////////////////////////////////////////////////////////

/**
 * Return true if there is any item in any tab selected.
 */
bool BindingTargetSelector::isTargetSelected()
{
    bool selected = false;
    int tab = tabs.getCurrentTabIndex();
    if (tab >= 0) {
        SimpleListBox* box = boxes[tab];
        selected = (box->getSelectedRow() >= 0);
    }
    return selected;
}

/**
 * Return the name of the selected target, or empty string
 * if nothing is selected.
 */
juce::String BindingTargetSelector::getSelectedTarget()
{
    juce::String target;
    int tab = tabs.getCurrentTabIndex();
    if (tab >= 0) {
        SimpleListBox* box = boxes[tab];
        int row = box->getSelectedRow();
        if (row >= 0) {
            target = box->getSelectedValue();
        }
    }
    return target;
}

/**
 * This is called whenever the user manualy clicks on a row
 * AND when a row is selected programatically by things
 * like selectRow, deselectAllRows(), etc.
 *
 * BindingEditor needs to programatically select rows for
 * showSelectedTarget, and those must not call the listener because
 * BindingEditor uses the listener as a signal to reset the form
 * which we don't want.
 */
void BindingTargetSelector::selectedRowsChanged(SimpleListBox* box, int lastRow)
{
    (void)lastRow;
    // once a row is selected in one tab's box, the others are deselected
    deselectOtherTargets(box);
}

/**
 * Here via the ListBoxModel when the user clicks on a row.
 * This is enough for the current use of our listener.  The row is in
 * a selected state at this point, so the receiver can call back to
 * the SimpleListBox or to us to get the selection.
 */
void BindingTargetSelector::listBoxItemClicked(SimpleListBox* box, int row)
{
    (void)box;
    (void)row;
    if (listener != nullptr)
      listener->bindingTargetClicked(this);
}
    
void BindingTargetSelector::deselectOtherTargets(SimpleListBox* active)
{
    for (int i = 0 ; i < boxes.size() ; i++) {
        SimpleListBox* other = boxes[i];
        if (other != active)
          other->deselectAll();
    }
}

void BindingTargetSelector::reset()
{
    deselectOtherTargets(nullptr);
    showTab(0);
}

/**
 * Adjust the tabs and list boxes to display the
 * desired target.  The format of the name must
 * match what is returned by getSelectedTarget.
 */
void BindingTargetSelector::showSelectedTarget(juce::String name)
{
    bool found = false;

    // getting some weird lingering state that prevents
    // setSelectedRow after showing the selected tab from
    // highlighting, start by full deselection?
    // yes, this works
    // maybe if the row had been selected previously,
    // we moved to a different tab, then back again it
    // won't show it?  weird and I'm too tired to figure it out
    reset();
    
    for (int tab = 0 ; tab < tabs.getNumTabs() && !found ; tab++) {
        SimpleListBox* box = boxes[tab];
        
        int numRows = box->getNumRows();
        for (int row = 0 ; row < numRows ; row++) {
            juce::String value = box->getRowValue(row);
            if (name == value) {
                // this is the one
                showTab(tab);
                box->setSelectedRow(row);
                found = true;
                break;
            }
        }
    }

    if (!found) {
        // must have had an invalid name in the config file
        // clear any lingering target
        reset();
    }
}

/**
 * Tests to see if a target name is valid.
 * Used by binding panels to filter out stale data from
 * the config file.
 * Does the same awkward walk as showSelectedTarget but
 * not worth defining an iterator at the moment.
 *
 * update: this is probably obsolete after the introduction
 * of Symbols.  We'll intern symbols for invalid bindings
 * but can display them in red as unresolved.
 */
bool BindingTargetSelector::isValidTarget(juce::String name)
{
    bool valid = false;

    for (int tab = 0 ; tab < tabs.getNumTabs() ; tab++) {
        SimpleListBox* box = boxes[tab];
        
        int numRows = box->getNumRows();
        for (int row = 0 ; row < numRows ; row++) {
            juce::String value = box->getRowValue(row);
            if (name == value) {
                valid = true;
                break;
            }
        }
    }

    return valid;
}

/**
 * Capture the selected target into a binding.
 * This is much simpler now that all we have to do
 * is find and store the Symbol.
 */
void BindingTargetSelector::capture(Binding* b)
{
    juce::String name = getSelectedTarget();
    if (name.length() == 0) {
        // nothing selected, it keeps whatever it had
    }
    else {
        b->setSymbolName(name.toUTF8());
    }
}

/**
 * Given a binding, auto-select a tab and row to
 * bring the symbol name into view.
 *
 * todo: If this was hidden or unresolved, we may not have
 * anything to show and should display a message.
 */
void BindingTargetSelector::select(Binding* b)
{
    showSelectedTarget(b->getSymbolName());
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
