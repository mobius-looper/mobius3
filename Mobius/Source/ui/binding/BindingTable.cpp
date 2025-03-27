
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/MidiUtil.h"
#include "../../util/Util.h"
#include "../../model/Symbol.h"
#include "../../model/BindingSet.h"
#include "../../model/Binding.h"
#include "../../Supervisor.h"
#include "../../Producer.h"

#include "../common/YanPopup.h"
#include "../common/YanDialog.h"
#include "../common/YanField.h"

#include "BindingEditor.h"
#include "BindingTable.h"

BindingTable::BindingTable()
{
    setName("BindingTable");

    // sadly important that this be called
    TypicalTable::initialize();

    addColumn("Target", TargetColumn, 200);
    addColumn("Trigger", TriggerColumn, 200);
    addColumn("Arguments", ArgumentsColumn, 100);
    addColumn("Scope", ScopeColumn, 50);

    rowPopup.add("Edit...", DialogEdit);
    rowPopup.add("New...", DialogNew);
    rowPopup.add("Delete...", DialogDelete);

    emptyPopup.add("New...", DialogNew);

    newDialog.setTitle("New Binding");
    newDialog.setButtons("Ok,Cancel");

    // add ourselves as a MouseListener to pick up clicks outside the rows
    table.addMouseListener(this, false);
}

BindingTable::~BindingTable()
{
}

void BindingTable::load(BindingEditor* ed, BindingSet* set)
{
    editor = ed;
    bindingSet = set;
    reload();
}

void BindingTable::reload()
{
    bindingRows.clear();

    for (auto b : bindingSet->getBindings()) {
        BindingTableRow* row = new BindingTableRow();
        row->binding = b;
        bindingRows.add(row);
    }
    
    updateContent();
}

/**
 * This is called by the BindingSetEditor when it saves or cancels.
 * Forget everything you know since the object we've been editing is
 * no longer stable.
 */
void BindingTable::clear()
{
    bindingSet = nullptr;
    bindingRows.clear();
}

void BindingTable::cancel()
{
    clear();
    // make sure all of the dialogs are gone
    newDialog.cancel();
    // popups too?
}

//////////////////////////////////////////////////////////////////////
//
// TypicalTable Overrides
//
//////////////////////////////////////////////////////////////////////

int BindingTable::getRowCount()
{
    return bindingRows.size();
}

juce::String BindingTable::getCellText(int rowNumber, int columnId)
{
    juce::String cell;

    BindingTableRow* row = bindingRows[rowNumber];
    if (row != nullptr) {
        Binding* b = row->binding;

        if (columnId == TargetColumn) {
            cell = b->symbol;
        }
        else if (columnId == TriggerColumn) {
            cell = renderTrigger(b);
        }
        else if (columnId == ScopeColumn) {
            cell = renderScope(b);
        }
        else if (columnId == ArgumentsColumn) {
            cell = b->arguments;
        }
        else if (columnId == DisplayNameColumn) {
            cell = b->displayName;
        }
    }
    
    return cell;
}

juce::String BindingTable::renderTrigger(Binding* b)
{
    juce::String text;
    Binding::Trigger trigger = b->trigger;

    // the menu displays channels as one based, not sure what
    // most people expect
    juce::String channel;
    if (b->midiChannel > 0)
      channel = juce::String(b->midiChannel) + ":";
    
    if (trigger == Binding::TriggerNote) {
        // old utility
        char buf[32];
        MidiNoteName(b->triggerValue, buf);
        text += channel;
        text += buf;
        // not interested in velocity
    }
    else if (trigger == Binding::TriggerProgram) {
        text += channel;
        text += "Pgm ";
        text += juce::String(b->triggerValue);
    }
    else if (trigger == Binding::TriggerControl) {
        text += channel;
        text += "CC ";
        text += juce::String(b->triggerValue);
    }
    else if (trigger == Binding::TriggerKey) {
        // unpack our compressed code/modifiers value
        int code;
        int modifiers;
        Binderator::unpackKeyQualifier(b->triggerValue, &code, &modifiers);
        return KeyTracker::getKeyText(code, modifiers);
    }
    else {
        text = "???";
    }
    
    return text;
}

/**
 * I think the old way stored these as text and they were
 * parsed at runtime into the mTrack and mGroup numbers
 * Need a lot more here as we refine what scopes mean.
 */ 
juce::String BindingTable::renderScope(Binding* b)
{
    if (b->scope == "")
      return juce::String("Global");
    else
      return b->scope;
}

void BindingTable::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
      rowPopup.show();
    else
      TypicalTable::cellClicked(rowNumber, columnId, event);
}

void BindingTable::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    BindingTableRow* row = bindingRows[rowNumber];
    if (row != nullptr) {
        if (editor != nullptr)
          editor->showBinding(row->binding);
    }
}

/**
 * gag, really messy disconnect between what is a table Listener and what is a subclass
 * Tables in general are really pissing me off.  Design something that just works.
 *
 * Here, TypicalTable added a MouseListener on the inner TableListBox to get notified when
 * the mouse is clicked on empty space below the rows.  It calls the Listener typicalTableSpaceClicked
 * when that happens, but we aren't a table listener since we subclass TypicalTable.
 *
 * I suppose we could just add ourselves as the MouseListener, but I'd really like to move toward
 * a more opaque table model rather than this dumb subclassing.
 */
void BindingTable::mouseDown(const juce::MouseEvent& event)
{
    // will actually want a different popup here that doesn't have Delete
    if (event.mods.isRightButtonDown())
      emptyPopup.show();
}

//////////////////////////////////////////////////////////////////////
//
// Menu Handlers and Dialogs
//
//////////////////////////////////////////////////////////////////////

void BindingTable::yanPopupSelected(YanPopup* src, int id)
{
    (void)src;
    
    Dialog dialog = (Dialog)id;
    switch (dialog) {
        
        case DialogEdit:
            break;
            
        case DialogNew:
            startNew();
            break;
            
        case DialogDelete:
            break;
    }
}

void BindingTable::startNew()
{
    newDialog.setTitle("Select Binding Type");
    newDialog.setId(DialogNew);
    newDialog.show(getParentComponent());
}

void BindingTable::yanDialogClosed(YanDialog* d, int button)
{
    int id = d->getId();
    switch (id) {
        case DialogNew: finishNew(button); break;
    }
}

void BindingTable::finishNew(int button)
{
    if (button == 0) {
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
