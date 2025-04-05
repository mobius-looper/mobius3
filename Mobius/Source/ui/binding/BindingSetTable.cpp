
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../model/Symbol.h"
#include "../../model/BindingSets.h"
#include "../../model/ValueSet.h"
#include "../../Supervisor.h"
#include "../../Producer.h"

#include "../common/YanPopup.h"
#include "../common/YanDialog.h"
#include "../common/YanField.h"

#include "BindingEditor.h"
#include "BindingSetTable.h"

BindingSetTable::BindingSetTable(BindingEditor* e)
{
    editor = e;
    setName("BindingSetTable");

    initialize();

    if (e->isButtons())
      objectTypeName = "Button Set";
    else
      objectTypeName = "Binding Set";

    addColumn(objectTypeName, ColumnName, 200);

    // activation/deactivation doesn't work yet, you
    // have to select them as the sessionBindingSet or trackBindingSet in the
    // session editor
    //rowPopup.add("Activate", DialogActivate);
    //rowPopup.add("Deactivate", DialogDeactivate);
    
    rowPopup.add("Copy...", DialogCopy);
    rowPopup.add("New...", DialogNew);
    rowPopup.add("Properties...", DialogProperties);
    rowPopup.add("Delete...", DialogDelete);
    rowPopup.add("Help...", DialogHelp);
    
    emptyPopup.add("New...", DialogNew);
    emptyPopup.add("Help...", DialogHelp);

    nameDialog.setButtons("Ok,Cancel");
    nameDialog.addField(&newName);
    
    propertiesDialog.setButtons("Ok,Cancel");
    propertiesDialog.addField(&propName);
    if (!(e->isButtons()))
      propertiesDialog.addField(&propOverlay);
    
    deleteAlert.setButtons("Delete,Cancel");
    deleteAlert.setSerious(true);
    deleteAlert.addMessage("Are you sure you want to delete this set?");
    deleteAlert.addMessage("This cannot be undone");
    
    confirmDialog.setTitle("Confirm");
    confirmDialog.setButtons("Ok,Cancel");
    confirmDialog.addMessage("Are you sure you want to do that?");
    
    errorAlert.addButton("Ok");
    errorAlert.setSerious(true);

    // add ourselves as a MouseListener to pick up clicks outside the rows
    table.addMouseListener(this, false);
}

BindingSetTable::~BindingSetTable()
{
}

void BindingSetTable::load(BindingSets* sets)
{
    bindingSets = sets;
    reload();
}

void BindingSetTable::reload()
{
    bindingSetRows.clear();

    for (auto set : bindingSets->getSets()) {
        if (set->name.length() == 0) {
            Trace(1, "BindingSetTable: ValueSet without a name");
        }
        else {
            BindingSetTableRow* row = new BindingSetTableRow();
            row->set = set;
            bindingSetRows.add(row);
        }
    }
    
    updateContent();
}

void BindingSetTable::refresh()
{
    updateContent();
}

/**
 * This is called by the BindingSetEditor when it saves or cancels.'
 * Forget everything you know since the object we've been editing is
 * no longer stable.
 */
void BindingSetTable::clear()
{
    bindingSets = nullptr;
    bindingSetRows.clear();
}

void BindingSetTable::cancel()
{
    // make sure all of the dialogs are gone
    nameDialog.cancel();
    propertiesDialog.cancel();
    deleteAlert.cancel();
    confirmDialog.cancel();
    errorAlert.cancel();

    // popups too?
}

//////////////////////////////////////////////////////////////////////
//
// TypicalTable Overrides
//
//////////////////////////////////////////////////////////////////////

int BindingSetTable::getRowCount()
{
    return bindingSetRows.size();
}

juce::String BindingSetTable::getCellText(int rowNumber, int columnId)
{
    juce::String cell;
    
    BindingSetTableRow* row = bindingSetRows[rowNumber];

    if (columnId == ColumnName) {
        cell = row->set->name;
        if (row->set->overlay)
          cell += " (overlay)";
    }

    return cell;
}

void BindingSetTable::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
      rowPopup.show();
    else
      TypicalTable::cellClicked(rowNumber, columnId, event);
}

void BindingSetTable::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)rowNumber;
    (void)columnId;
    (void)event;
    startProperties();
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
void BindingSetTable::mouseDown(const juce::MouseEvent& event)
{
    // will actually want a different popup here that doesn't have Delete
    if (event.mods.isRightButtonDown())
      emptyPopup.show();
}

/**
 * TableListBoxModel override
 */
void BindingSetTable::deleteKeyPressed(int lastRowSelected)
{
    (void)lastRowSelected;
    startDelete();
}

void BindingSetTable::returnKeyPressed(int lastRowSelected)
{
    (void)lastRowSelected;
    startProperties();
}

//////////////////////////////////////////////////////////////////////
//
// Menu Handlers and Dialogs
//
//////////////////////////////////////////////////////////////////////

void BindingSetTable::yanPopupSelected(YanPopup* src, int id)
{
    (void)src;
    
    Dialog dialog = (Dialog)id;
    switch (dialog) {
        case DialogCopy: startCopy(); break;
        case DialogNew: startNew(); break;
        case DialogProperties: startProperties(); break;
        case DialogDelete: startDelete(); break;
        case DialogHelp: break;
    }
}

void BindingSetTable::startNew()
{
    propertiesDialog.setTitle(juce::String("Create New ") + objectTypeName);
    propertiesDialog.setId(DialogNew);
    propName.setValue("");
    propOverlay.setValue(false);
    propertiesDialog.show(getParentComponent());
}

void BindingSetTable::startCopy()
{
    nameDialog.setTitle(juce::String("Copy ") + objectTypeName);
    nameDialog.setId(DialogCopy);
    newName.setValue("");
    nameDialog.show(getParentComponent());
}

void BindingSetTable::startProperties()
{
    propertiesDialog.setTitle(objectTypeName + juce::String(" Properties"));
    propertiesDialog.setId(DialogProperties);
    BindingSet* set = getSelectedSet();
    if (set != nullptr) {
        propName.setValue(set->name);
        propOverlay.setValue(set->overlay);
        propertiesDialog.show(getParentComponent());
    }
}

void BindingSetTable::startDelete()
{
    deleteAlert.setTitle(juce::String("Delete ") + objectTypeName);
    deleteAlert.setId(DialogDelete);
    deleteAlert.show(getParentComponent());
}

void BindingSetTable::yanDialogClosed(YanDialog* d, int button)
{
    int id = d->getId();
    switch (id) {
        case DialogNew: finishNew(button); break;
        case DialogCopy: finishCopy(button); break;
        case DialogProperties: finishProperties(button); break;
        case DialogDelete: finishDelete(button); break;
        case DialogHelp: break;
    }
}

BindingSet* BindingSetTable::getSelectedSet()
{
    BindingSet* result = nullptr;
    int rownum = getSelectedRow();
    if (rownum >= 0) {
        BindingSetTableRow* row = bindingSetRows[rownum];
        if (row != nullptr)
          result = row->set;
    }
    return result;
}

void BindingSetTable::finishNew(int button)
{
    if (button == 0) {

        BindingSet* neu = new BindingSet();
        neu->name = propName.getValue();
        neu->overlay = propOverlay.getValue();
        
        juce::StringArray errors;
        editor->bindingSetTableNew(neu, errors);
        showResult(errors);
    }
}

void BindingSetTable::finishCopy(int button)
{
    if (button == 0) {
        juce::StringArray errors;
        editor->bindingSetTableCopy(newName.getValue(), errors);
        showResult(errors);
    }
}

void BindingSetTable::finishProperties(int button)
{
    if (button == 0) {
        juce::StringArray errors;

        // !! we've asynchronously left the table and shown a dialog
        // when the dialog closes there is no guarantee that the original
        // set will still be selected
        BindingSet* set = getSelectedSet();

        editor->checkName(set, propName.getValue(), errors);

        if (errors.size() > 0)
          showResult(errors);
        else {
            set->name = propName.getValue();
            set->overlay = propOverlay.getValue();
            refresh();
        }
    }
}

void BindingSetTable::finishDelete(int button)
{
    if (button == 0) {
        juce::StringArray errors;
        editor->bindingSetTableDelete(errors);
        showResult(errors);
    }
}

void BindingSetTable::showResult(juce::StringArray& errors)
{
    // obviously lots more we could do here
    if (errors.size() > 0) {
        errorAlert.clearMessages();
        for (auto e : errors)
          errorAlert.addMessage(e);

        errorAlert.setTitle(juce::String("Error saving ") + objectTypeName);
        errorAlert.show(getParentComponent());
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
