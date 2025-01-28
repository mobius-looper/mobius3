
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../model/Symbol.h"
#include "../../model/Session.h"
#include "../../Supervisor.h"
#include "../../Producer.h"

#include "../common/YanPopup.h"
#include "../common/YanDialog.h"
#include "../common/YanField.h"

#include "SessionManagerTable.h"

SessionManagerTable::SessionManagerTable(Supervisor* s)
{
    supervisor = s;
    setName("SessionManagerTable");

    initialize();

    addColumn("Name", ColumnName, 200);
    
    rowPopup.add("Load...", DialogLoad);
    rowPopup.add("Copy...", DialogCopy);
    rowPopup.add("New...", DialogNew);
    rowPopup.add("Rename...", DialogRename);
    rowPopup.add("Delete...", DialogDelete);

    emptyPopup.add("New...", DialogNew);

    nameDialog.setTitle("New Session");
    nameDialog.addField(&newName);
    nameDialog.addButton("Ok");
    nameDialog.addButton("Cancel");

    deleteAlert.setTitle("Delete Session");
    deleteAlert.setSerious(true);
    deleteAlert.setMessage("Are you sure you want to delete this session?");
    deleteAlert.addButton("Delete");
    deleteAlert.addButton("Cancel");
    
    confirmDialog.setTitle("Confirm");
    confirmDialog.setMessage("Are you sure you want to do that?");
    confirmDialog.addButton("Ok");
    confirmDialog.addButton("Cancel");
    
    invalidAlert.setTitle("Missing Name");
    invalidAlert.setSerious(true);
    invalidAlert.setMessage("A new name was not entered");
    invalidAlert.addButton("Ok");
    
    errorAlert.setTitle("Error Saving Session");
    errorAlert.setSerious(true);
    errorAlert.addButton("Ok");

    // add ourselves as a MouseListener to pick up clicks outside the rows
    table.addMouseListener(this, false);
}

SessionManagerTable::~SessionManagerTable()
{
}

void SessionManagerTable::load()
{
    reload();
}

void SessionManagerTable::reload()
{
    sessions.clear();
    names.clear();
    
    Producer* producer = supervisor->getProducer();
    producer->getSessionNames(names);

    for (auto name : names) {
        SessionManagerTableRow* row = new SessionManagerTableRow();
        row->name = name;
        sessions.add(row);
    }
    
    updateContent();
    repaint();
}

// this doesn't make sense right?
void SessionManagerTable::clear()
{
    Trace(1, "SessionManagerTable::clear Who is calling this?");
    //results.clear();
    //updateContent();
}

//////////////////////////////////////////////////////////////////////
//
// TypicalTable Overrides
//
//////////////////////////////////////////////////////////////////////

int SessionManagerTable::getRowCount()
{
    return sessions.size();
}

juce::String SessionManagerTable::getCellText(int rowNumber, int columnId)
{
    juce::String cell;
    
    SessionManagerTableRow* row = sessions[rowNumber];

    if (columnId == ColumnName) {
        cell = row->name;
    }

    return cell;
}

void SessionManagerTable::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
      rowPopup.show();
    else
      TypicalTable::cellClicked(rowNumber, columnId, event);
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
void SessionManagerTable::mouseDown(const juce::MouseEvent& event)
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

void SessionManagerTable::yanPopupSelected(YanPopup* src, int id)
{
    (void)src;
    
    Dialog dialog = (Dialog)id;
    switch (dialog) {
        case DialogLoad: startLoad(); break;
        case DialogCopy: startCopy(); break;
        case DialogNew: startNew(); break;
        case DialogRename: startRename(); break;
        case DialogDelete: startDelete(); break;
    }
}

void SessionManagerTable::startLoad()
{
    if (isSessionModified()) {
        confirmDialog.setTitle("Session Modified");
        confirmDialog.setMessage("The current session has unsaved changes, continue loading new session?");
        confirmDialog.setId(DialogLoad);
        confirmDialog.show(getParentComponent());
    }
}

void SessionManagerTable::startNew()
{
    nameDialog.setTitle("New Session");
    nameDialog.setId(DialogNew);
    newName.setValue("");
    nameDialog.show(getParentComponent());
}

void SessionManagerTable::startCopy()
{
    nameDialog.setTitle("Copy Session");
    nameDialog.setId(DialogCopy);
    newName.setValue("");
    nameDialog.show(getParentComponent());
}

void SessionManagerTable::startRename()
{
    nameDialog.setTitle("Rename Session");
    nameDialog.setId(DialogRename);

    newName.setValue(getSelectedName());
    
    nameDialog.show(getParentComponent());
}

void SessionManagerTable::startDelete()
{
    deleteAlert.setId(DialogDelete);
    
    deleteAlert.show(getParentComponent());
}

void SessionManagerTable::yanDialogClosed(YanDialog* d, int button)
{
    int id = d->getId();
    switch (id) {
        case DialogLoad: finishLoad(button); break;
        case DialogNew: finishNew(button); break;
        case DialogCopy: finishCopy(button); break;
        case DialogRename: finishRename(button); break;
        case DialogDelete:finishDelete(button); break;
    }
}

juce::String SessionManagerTable::getSelectedName()
{
    int rownum = getSelectedRow();
    SessionManagerTableRow* row = sessions[rownum];
    return row->name;
}

void SessionManagerTable::finishLoad(int button)
{
    if (button == 0) {
        juce::String name = getSelectedName();
        if (name.length() > 0) {
            Trace(2, "SessionManagerTable: Session loaded");
        }
    }
}

void SessionManagerTable::finishNew(int button)
{
    if (button == 0) {
        juce::String name = newName.getValue();
        if (validateName(name)) {
            Producer* p = supervisor->getProducer();
            juce::String error = p->createSession(name);
            if (checkErrors(error)) {
                Trace(2, "SessionManagerTable: Session copied");
                reload();
            }
        }
    }
}

void SessionManagerTable::finishCopy(int button)
{
    if (button == 0) {
        juce::String name = newName.getValue();
        if (validateName(name)) {
            Producer* p = supervisor->getProducer();
            juce::String error = p->copySession(getSelectedName(), name);
            if (checkErrors(error)) {
                Trace(2, "SessionManagerTable: Session copied");
                reload();
            }
        }
    }
}

void SessionManagerTable::finishRename(int button)
{
    if (button == 0) {
        juce::String name = newName.getValue();
        if (validateName(name)) {
            Producer* p = supervisor->getProducer();
            juce::String error = p->renameSession(getSelectedName(), name);
            if (checkErrors(error)) {
                Trace(2, "SessionManagerTable: Session renamed");
                reload();
            }
        }
    }
}

void SessionManagerTable::finishDelete(int button)
{
    if (button == 0) {
        Producer* p = supervisor->getProducer();
        juce::String error = p->deleteSession(getSelectedName());
        if (checkErrors(error)) {
            Trace(2, "SessionManagerTable: Session deleted");
            reload();
        }
    }
}

bool SessionManagerTable::validateName(juce::String name)
{
    bool valid = false;
    
    if (name.length() == 0) {
        invalidAlert.setTitle("Missing Name");
        invalidAlert.setMessage("A new name was not entered");
        invalidAlert.show(getParentComponent());
    }
    else if (names.contains(name)) {
        invalidAlert.setTitle("Duplicate Name");
        invalidAlert.setMessage("The entered name is already in use");
        invalidAlert.show(getParentComponent());
    }
    else if (hasInvalidCharacters(name)) {
        invalidAlert.setTitle("Invalid Name");
        invalidAlert.setMessage("The entered name contained illegal characters");
        invalidAlert.show(getParentComponent());
    }
    else {
        valid = true;
    }
    return valid;
}

bool SessionManagerTable::hasInvalidCharacters(juce::String name)
{
    return name.containsAnyOf("\\/$.");
}

bool SessionManagerTable::isSessionModified()
{
    return false;
}

bool SessionManagerTable::checkErrors(juce::String error)
{
    bool ok = true;
    if (error.length() > 0) {
        errorAlert.setMessage(error);
        errorAlert.show(getParentComponent());
        ok = false;
    }
    return ok;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

