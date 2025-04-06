
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../model/Symbol.h"
#include "../../model/BindingSet.h"
#include "../../model/Binding.h"
#include "../../Supervisor.h"
#include "../../Producer.h"

#include "../common/YanPopup.h"
#include "../common/YanDialog.h"
#include "../common/YanField.h"

#include "BindingUtil.h"
#include "BindingEditor.h"
#include "BindingTable.h"

BindingTable::BindingTable()
{
    setName("BindingTable");

    // sadly important that this be called
    TypicalTable::initialize();

    rowPopup.add("Edit...", DialogEdit);
    rowPopup.add("Delete...", DialogDelete);
    rowPopup.add("Help...", DialogHelp);

    emptyPopup.add("Help...", DialogHelp);

    //newDialog.setTitle("New Binding");
    //newDialog.setButtons("Ok,Cancel");

    helpDialog.setTitle("Binding Help");
    helpDialog.setButtons("Ok");
    
    // add ourselves as a MouseListener to pick up clicks outside the rows
    table.addMouseListener(this, false);
}

BindingTable::~BindingTable()
{
}

void BindingTable::load(BindingEditor* ed, BindingSet* set, Type t)
{
    editor = ed;
    bindingSet = set;
    type = t;

    if (type == TypeButton) {
        addColumn("Target", TargetColumn, 200);
        addColumn("Arguments", ArgumentsColumn, 100);
        addColumn("Send To", ScopeColumn, 50);
    }
    else {
        addColumn("Target", TargetColumn, 200);
        addColumn("Trigger", TriggerColumn, 200);
        addColumn("Arguments", ArgumentsColumn, 100);
        addColumn("Send To", ScopeColumn, 50);
    }
    
    reload();
}

void BindingTable::reload()
{
    bindingRows.clear();

    for (auto b : bindingSet->getBindings()) {

        if (type == TypeMidi) {
            if (b->trigger == Binding::TriggerNote ||
                b->trigger == Binding::TriggerControl ||
                b->trigger == Binding::TriggerProgram) {
                addBinding(b);
            }
        }
        else if (type == TypeKey) {
            if (b->trigger == Binding::TriggerKey) {
                addBinding(b);
            }
        }
        else if (type == TypeHost) {
            if (b->trigger == Binding::TriggerHost) {
                addBinding(b);
            }
        }
        else if (type == TypeButton) {
            // don't need to filter, they'll all be buttons
            addBinding(b);
        }
    }
    
    updateContent();
}

void BindingTable::refresh()
{
    updateContent();
}

void BindingTable::add(Binding* b)
{
    addBinding(b);
    updateContent();
}

void BindingTable::addBinding(Binding* b)
{
    BindingTableRow* row = new BindingTableRow();
    row->binding = b;
    
    if (type == TypeButton) {
        bindingRows.add(row);
    }
    else {
        BindingTableComparator comparator;
        bindingRows.addSorted(comparator, row);
        // todo: need to find it and select it
    }
}

void BindingTable::addAndEdit(Binding* b)
{
    addBinding(b);
    if (editor != nullptr)
      editor->showBinding(b);
}

int BindingTableComparator::compareElements(BindingTableRow* first, BindingTableRow* second)
{
    juce::String name1 = first->binding->symbol;
    juce::String name2 = second->binding->symbol;
    return name1.compareIgnoreCase(name2);
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
    //newDialog.cancel();
    // popups too?
    helpDialog.cancel();
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
            cell = BindingUtil::renderTrigger(b);
        }
        else if (columnId == ScopeColumn) {
            cell = BindingUtil::renderScope(b);
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

void BindingTable::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
      rowPopup.show();
    else
      TypicalTable::cellClicked(rowNumber, columnId, event);
}

void BindingTable::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)columnId;
    (void)event;
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
        
        case DialogEdit: {
            int selected = getSelectedRow();
            if (selected >= 0) {
                BindingTableRow* row = bindingRows[selected];
                if (row != nullptr) {
                    if (editor != nullptr)
                      editor->showBinding(row->binding);
                }
            }
        }
            break;
            
        case DialogDelete: {
            deleteCurrent();
        }
            break;
            
        case DialogHelp:
            startHelp();
            break;
    }
}

/**
 * todo: It might be nice to keep this Binding in on an undo list
 * so it can be restored if you deleted it accidentally without having
 * to cancel the entire binding editor and reload it.
 */
void BindingTable::deleteCurrent()
{
    int selected = getSelectedRow();
    if (selected >= 0) {
        BindingTableRow* row = bindingRows[selected];
        if (row != nullptr) {
            bindingSet->remove(row->binding);
            bindingRows.removeObject(row, true);
            refresh();
        }
    }
}

/**
 * TableListBoxModel override
 */
void BindingTable::deleteKeyPressed(int lastRowSelected)
{
    // the use of the words "lastRow" is disturbing, is this ever
    // different than the current row?
    int current = getSelectedRow();
    if (current == lastRowSelected)
      deleteCurrent();
    else
      Trace(1, "BindingTable::deleteKeyPressed row number mismatch");
}

void BindingTable::returnKeyPressed(int lastRowSelected)
{
    (void)lastRowSelected;
    int current = getSelectedRow();
    if (current >= 0) {
        BindingTableRow* row = bindingRows[current];
        if (row != nullptr) {
            if (editor != nullptr)
              editor->showBinding(row->binding);
        }
    }
}

void BindingTable::startHelp()
{
    helpDialog.setTitle("Binding Table Help");
    helpDialog.setId(DialogHelp);
    helpDialog.show(getParentComponent());
}

void BindingTable::yanDialogClosed(YanDialog* d, int button)
{
    (void)d;
    (void)button;
}

//////////////////////////////////////////////////////////////////////
//
// Reorder Hacking
//
// !! all this shit was copied from SessionTrackTable
// when you get around to redesigning TypicalTable, ordered rows needs
// to be built in to that
//
//////////////////////////////////////////////////////////////////////

/**
 * Build the thing the target gets when something is dropped from this table.
 * Since we're the only one interested in drops from ourselves, pass
 * the row number.
 */
juce::var BindingTable::getDragSourceDescription (const juce::SparseSet<int>& selectedRows)
{
    juce::String desc;
    if (type == TypeButton) {
        if (selectedRows.size() > 1) {
            Trace(1, "BindingTable: Trying to drag more than one row");
        }
        else {
            desc = juce::String(DragPrefix) + juce::String(selectedRows[0]);
        }
    }
    return desc;
}

/**
 * Interested in drops only for the button table,
 */
bool BindingTable::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)details;
    return (type == TypeButton);
}

/**
 * When something comes in, start dragging the drop location with a little
 * line between rows.
 *
 * This is drawn by TypicalTable when the paintDropTarget fla is set and
 * dropTargetRow has the row number.
 */
void BindingTable::itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)details;
    if (type == TypeButton) {
        paintDropTarget = true;
        // should be initialized but make sure and wait for an itemDragMove
        dropTargetRow = -1;
    }
}

/**
 * As the drag moves within the table, draw the insertion point as with a line
 * between rows.  getInsertIndexForPosition handles the viewport calculations
 * and picks a good index, using the center of the row as the transition point.
 */
void BindingTable::itemDragMove(const juce::DragAndDropTarget::SourceDetails& details)
{
    if (type == TypeButton) {
        juce::Point<int> pos = details.localPosition;

        int listBoxX = pos.getX() - table.getX();
        int listBoxY = pos.getY() - table.getY();
        int insertIndex =  table.getInsertionIndexForPosition(listBoxX, listBoxY);
        
        if (insertIndex != dropTargetRow) {
            //Trace(2, "Insertion index %d", insertIndex);
            dropTargetRow =  insertIndex;
            repaint();
        }
    }
}

/**
 * The drag went off into space.
 * For some tables, this could indicate a removal of the row.
 * I'd rather not do that for bindings since it's easy to do accidentally.
 */
void BindingTable::itemDragExit (const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)details;
    if (type == TypeButton) {
        //Trace(2, "BindingTable::itemDragExit");
        paintDropTarget = false;
        dropTargetRow = -1;
        repaint();
    }
}

/**
 * Something dropped in this ListBox.
 * Since we are both a source and a target, if we drop within oursleves,
 * treat this as a move if the list is ordered.
 *
 * If we are dragging from the outside, convert the source details into
 * a StringArray, add those values to our list, and inform the listener that
 * new values were received.  When used with MultiSelectDrag this will cause
 * those values to be removed from the source ListBox.
 *
 * NOTE WELL:
 *
 * Even though BindingTable is used for both Buttons and Midi bindings, you
 * won't actually get here when dropping symbols in the BindingEditor.  That's
 * because BindingEditor defines it's own DragAndDropTarget and it sorts so
 * the dropTargetRow tracking won't have been done.
 *
 * This is messy, and will get moreso when TypicalTable gets redesigned.
 * The table should be handling all drop requests and forwarding to the listener
 * to do any special handling it might require.
 */
void BindingTable::itemDropped (const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)details;
    if (type ==  TypeButton && dropTargetRow >= 0) {

        // and who dropped it
        juce::String dropSource = details.description.toString();

        if (dropSource.startsWith(BindingTree::DragPrefix)) {
            // dragging something from the parameter tree, insert new binding
            juce::String sname = dropSource.fromFirstOccurrenceOf(BindingTree::DragPrefix, false, false);
            Symbol* s = editor->getProvider()->getSymbols()->find(sname);
            if (s == nullptr)
              Trace(1, "BindingTable: Invalid symbol name in drop %s", sname.toUTF8());
            else {
                doInsert(s, dropTargetRow);
            }
        }
        else if (dropSource.startsWith(BindingTable::DragPrefix)) {
            // dragging onto ourselves, a row move
            juce::String srow = dropSource.fromFirstOccurrenceOf(BindingTable::DragPrefix, false, false);
            if (srow.length() > 0) {
                int dragRow = srow.getIntValue();
                doMove(dragRow, dropTargetRow);
            }
        }
        else {
            Trace(1, "BindingTable: Unknown drop source %s",
                  dropSource.toUTF8());
        }

        // stop drawing insert points
        paintDropTarget = false;
        dropTargetRow = -1;

        // do this even if we decided not to move to get rid of the drop markers
        repaint();
    }
}

/**
 * Handle a drop from the parameter tree.
 */
void BindingTable::doInsert(Symbol* s, int dropRow)
{
    Binding* neu = new Binding();
    neu->symbol = s->name;

    switch (type) {
        case TypeMidi: neu->trigger = Binding::TriggerNote; break;
        case TypeKey: neu->trigger = Binding::TriggerKey; break;
        case TypeHost: neu->trigger = Binding::TriggerHost; break;
        case TypeButton: neu->trigger = Binding::TriggerUI; break;
    }

    // here is where we have the dual model problems
    // the BindingSet is authorative over order so we have to insert
    // into that and then rebuild the row model, there would be less
    // churn if we let the table control order, just make a Binding
    // and a BindingTableRow and insert that, then rebuild the BindingSet
    // list when it is saved. 

    juce::OwnedArray<class Binding>& bindings = bindingSet->getBindings();
    bindings.insert(dropRow, neu);

    // could do this with an incremental add if you were smarter
    // but this gets the job done
    reload();
    selectRow(dropRow);

    // expectation is immediate edit
    // todo: here there is an expectation problem
    // if the user selects Cancel from the popup then they may expect
    // that the add doesn't happen, but the row is still there
    // hmm: for buttons this is actually unnecessary since the only things
    // you can change are the scope and display name which are uncommon
    //if (editor != nullptr)
    //editor->showBinding(neu);
}

/**
 * For ordered tables only, move a row to a new location.
 *
 * SessionTrackEditor has some mind numbing logic due to the way
 * the drop row was calculated.  Simplified here.
 */
void BindingTable::doMove(int sourceRow, int dropRow)
{
    Trace(2, "BindingTable: Move row %d to %d", sourceRow, dropRow);
    if (dropRow >= 0 && dropRow != sourceRow) {
        moveBinding(sourceRow, dropRow);
    }
}

/**
 * This is complicated due to the way juce::Array::move works and
 * needs to take into account whether the new location is above
 * or below the starting location.
 *
 * Still don't fully undestand this, but testing shows this method worked.
 *
 * sourceRow is the track INDEX it wants to move and
 * desiredRow is the index the track should have.
 *
 * oddment:  The sourceRow is the selected row or the row you are ON
 * and want to move, and desiredRow is the row you were over when the
 * mouse was released and where you want it to BE.  The way it seems to work
 * is in two phases, first remove row 0 which shifts everything up.  Then insert
 * the removed row back into the list.  Because of this upward shift, the insertion
 * index needs to be one less than what the drop target was.  Or something like that,
 * maybe I'm just not mathing this right.   Anyway, if you're moving up the
 * two indexes work, but if you're moving down you have to -1 the desiredRow.
 */
void BindingTable::moveBinding(int sourceRow, int desiredRow)
{
    if (sourceRow != desiredRow) {

        int adjustedRow = desiredRow;
        if (desiredRow > sourceRow)
          adjustedRow--;

        juce::OwnedArray<class Binding>& bindings = bindingSet->getBindings();

        if (sourceRow != adjustedRow &&
            sourceRow >= 0 && sourceRow < bindings.size() &&
            adjustedRow >= 0 && adjustedRow < bindings.size()) {

            bindings.move(sourceRow, adjustedRow);
            reload();
            selectRow(adjustedRow);
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
