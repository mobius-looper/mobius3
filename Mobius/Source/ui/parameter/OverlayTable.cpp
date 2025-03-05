
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../model/Symbol.h"
#include "../../model/ParameterSets.h"
#include "../../model/ValueSet.h"
#include "../../Supervisor.h"
#include "../../Producer.h"

#include "../common/YanPopup.h"
#include "../common/YanDialog.h"
#include "../common/YanField.h"

#include "OverlayTable.h"

OverlayTable::OverlayTable(Supervisor* s)
{
    supervisor = s;
    producer = s->getProducer();
    setName("OverlayTable");

    initialize();

    addColumn("Name", ColumnName, 200);
    
    rowPopup.add("Activate", DialogActivate);
    rowPopup.add("Deactivate", DialogDeactivate);
    rowPopup.add("Copy...", DialogCopy);
    rowPopup.add("New...", DialogNew);
    rowPopup.add("Rename...", DialogRename);
    rowPopup.add("Delete...", DialogDelete);

    emptyPopup.add("New...", DialogNew);

    nameDialog.setTitle("New Parameter Set");
    nameDialog.setButtons("Ok,Cancel");
    nameDialog.addField(&newName);

    deleteAlert.setTitle("Delete Parameter Set");
    deleteAlert.setButtons("Delete,Cancel");
    deleteAlert.setSerious(true);
    deleteAlert.addMessage("Are you sure you want to delete this session?");
    deleteAlert.addMessage("This cannot be undone");
    
    confirmDialog.setTitle("Confirm");
    confirmDialog.setButtons("Ok,Cancel");
    confirmDialog.addMessage("Are you sure you want to do that?");
    
    errorAlert.setTitle("Error Saving Parameter Set");
    errorAlert.addButton("Ok");
    errorAlert.setSerious(true);

    // add ourselves as a MouseListener to pick up clicks outside the rows
    table.addMouseListener(this, false);
}

OverlayTable::~OverlayTable()
{
}

void OverlayTable::load(ParameterSets* sets)
{
    overlays = sets;
    reload();
}

void OverlayTable::reload()
{
    overlayRows.clear();

    for (auto set : overlays->getSets()) {
        if (set->name.length() == 0) {
            Trace(1, "OverlayTable: ValueSet without a name");
        }
        else {
            OverlayTableRow* row = new OverlayTableRow();
            row->name = set->name;
            overlayRows.add(row);
        }
    }
    
    updateContent();
}

// this doesn't make sense right?
void OverlayTable::clear()
{
    Trace(1, "OverlayTable::clear Who is calling this?");
}

//////////////////////////////////////////////////////////////////////
//
// TypicalTable Overrides
//
//////////////////////////////////////////////////////////////////////

int OverlayTable::getRowCount()
{
    return overlayRows.size();
}

juce::String OverlayTable::getCellText(int rowNumber, int columnId)
{
    juce::String cell;
    
    OverlayTableRow* row = overlayRows[rowNumber];

    if (columnId == ColumnName) {
        cell = row->name;
    }

    return cell;
}

void OverlayTable::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
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
void OverlayTable::mouseDown(const juce::MouseEvent& event)
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

void OverlayTable::yanPopupSelected(YanPopup* src, int id)
{
    (void)src;
    
    Dialog dialog = (Dialog)id;
    switch (dialog) {
        case DialogActivate: doActivate(); break;
        case DialogDeactivate: doDeactivate(); break;
        case DialogCopy: startCopy(); break;
        case DialogNew: startNew(); break;
        case DialogRename: startRename(); break;
        case DialogDelete: startDelete(); break;
    }
}

void OverlayTable::doActivate()
{
}

void OverlayTable::doDeactivate()
{
}

void OverlayTable::startNew()
{
    nameDialog.setTitle("Create New Parameter Set");
    nameDialog.setId(DialogNew);
    newName.setValue("");
    nameDialog.show(getParentComponent());
}

void OverlayTable::startCopy()
{
    nameDialog.setTitle("Copy Parameter Set");
    nameDialog.setId(DialogCopy);
    newName.setValue("");
    nameDialog.show(getParentComponent());
}

void OverlayTable::startRename()
{
    nameDialog.setTitle("Rename Parmeter Set");
    nameDialog.setId(DialogRename);

    newName.setValue(getSelectedName());
    
    nameDialog.show(getParentComponent());
}

void OverlayTable::startDelete()
{
    deleteAlert.setId(DialogDelete);

    deleteAlert.show(getParentComponent());
}

void OverlayTable::yanDialogClosed(YanDialog* d, int button)
{
    int id = d->getId();
    switch (id) {
        case DialogNew: finishNew(button); break;
        case DialogCopy: finishCopy(button); break;
        case DialogRename: finishRename(button); break;
        case DialogDelete: finishDelete(button); break;
    }
}

juce::String OverlayTable::getSelectedName()
{
    int rownum = getSelectedRow();
    OverlayTableRow* row = overlayRows[rownum];
    return row->name;
}

void OverlayTable::finishNew(int button)
{
    if (button == 0) {
        juce::String name = newName.getValue();
        ValueSet* neu = new ValueSet();
        neu->name = name;
        overlays->add(neu);
        reload();
    }
}

void OverlayTable::finishCopy(int button)
{
    if (button == 0) {
        juce::String name = newName.getValue();

        // actually need to find the source and copy it!!
        ValueSet* neu = new ValueSet();
        neu->name = name;
        overlays->add(neu);

        reload();
    }
}

void OverlayTable::finishRename(int button)
{
    if (button == 0) {
        juce::String name = newName.getValue();
        reload();
    }
}

void OverlayTable::finishDelete(int button)
{
    if (button == 0) {
        reload();
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
