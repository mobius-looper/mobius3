
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
        addColumn("Scope", ScopeColumn, 50);
    }
    else {
        addColumn("Target", TargetColumn, 200);
        addColumn("Trigger", TriggerColumn, 200);
        addColumn("Arguments", ArgumentsColumn, 100);
        addColumn("Scope", ScopeColumn, 50);
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
    }
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

bool BindingTable::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)details;
    if (type == TypeButton)
      return true;
    else
      return false;
}

void BindingTable::itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details)
{
    if (type == TypeButton) {
        // we are both a source and a target, so don't highlight if we're over oursleves
        // spec is unclear what the sourceComponent will be if this is an item from
        // a ListBox, what you are dragging is some sort of inner component for the ListBox
        // with arbitrary structure between it and the ListBox, comparing against the
        // outer ListBox seems to work

        // !! why would this ever want to support drop from outside?
        if (details.sourceComponent != &table) {
            Trace(2, "BindingTable::itemDragEnter From outside");
            targetActive = true;
            moveActive = false;
        }
        else {
            //Trace(2, "BindingTable::itemDragEnter From inside");
            // moving within ourselves
            moveActive = true;
            targetActive = false;
        }
        paintDropTarget = true;
    }
}

/**
 * If we're dragging within ourselves, give some indication of the insertion point.
 * Actually it doesn't matter if the drag is coming from the outside, still need
 * to be order sensitive unless sorted.  I gave up trying to predict what
 * getInsertionIndexForPosition does.  You can calculate the drop position without that
 * in itemDropped, though it would be nice to draw that usual insertion line between items
 * while the drag is in progress.  Revisit someday...
 */
void BindingTable::itemDragMove(const juce::DragAndDropTarget::SourceDetails& details)
{
    if (type == TypeButton) {
        juce::Point<int> pos = details.localPosition;
        // position is "relative to the target component"  in this case the target
        // is the BindingTable which offsets the ListBox by 4 on all sides to draw
        // the drop border, convert wrapper coordinates to ListBox coordinates
        int listBoxX = pos.getX() - table.getX();
        int listBoxY = pos.getY() - table.getY();
        int insertIndex =  table.getInsertionIndexForPosition(listBoxX, listBoxY);
        if (insertIndex != lastInsertIndex) {
            //Trace(2, "Insertion index %ld\n", (long)insertIndex);
            lastInsertIndex = insertIndex;
        }

        // try this isntead
        int dropRow = getDropRow(details);
        if (dropRow != dropTargetRow) {
            dropTargetRow = dropRow;
            repaint();
        }
    }
}

/**
 * If we started a drag, and went off into space without landing on a target, I suppose we
 * could treat this as a special form of move that removes the value from the list.
 * But I don't think we can tell from here, this just means that the mouse left the
 * ListBox, it may come back again.
 */
void BindingTable::itemDragExit (const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)details;
    if (type == TypeButton) {
        Trace(2, "BindingTable::itemDragExit");
        targetActive = false;
        moveActive = false;
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
 */
void BindingTable::itemDropped (const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)details;
    if (type ==  TypeButton) {

        // here is where you would figure out the source row by comparing what
        // was placed in the details by getDragSourceDesription to one of the rows in the model
        //int sourceRow = strings.indexOf(newValues[0]);
    
        int dropRow = getDropRow(details);

        juce::String sourceThing = "???";;
        juce::var stuff = details.description;
        if (stuff.isArray()) {
            Trace(1, "BindingTable: Something dropped in an array");
        }
        else {
            sourceThing = stuff;
        }
    
#if 0    
        juce::String msg = juce::String("Dropped ") + sourceThing + juce::String(" into row ") +
            juce::String(dropRow);
        Trace(2, "BindingTable: %s", msg.toUTF8());
#endif
    
        if (sourceThing.length() > 0) {
            int sourceRow = sourceThing.getIntValue();
            if (doMove(sourceRow, dropRow)) {
                // we forward the move request to SessiontTrackEditor which
                // will normally call back to our reload() if it decided to do it
                //table.updateContent();
            }
        }

        targetActive = false;
        moveActive = false;
        paintDropTarget = false;
        dropTargetRow = -1;

        // do this even if we decided not to move to get rid of the drop markers
        repaint();
    }
}

/**
 * Calculate the row where a drop should be inserted when using
 * an unordered list.
 *
 * getInsertionIndexForPosition tracking during itemDragMove was wonky
 * and I never did understand it.  We don't really need that since we have
 * the drop coordinates in details.localPosition and can ask the ListBox
 * for getRowContainingPosition.  Note that localPosition is relative to the
 * DragAndDropTarget which is BindingTable and the ListBox is inset by
 * 4 on all sides to draw a border.  So have to adjust the coordinates to ListBox
 * coordinates when calling getRowCintainingPosition.
 */
int BindingTable::getDropRow(const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::Point<int> pos = details.localPosition;
    int dropX = pos.getX();
    int dropY = pos.getY();
    //Trace(2, "Drop position %ld %ld\n", dropX, dropY);
    dropX -= table.getX();
    dropY -= table.getY();
    //Trace(2, "Drop position within ListBox %ld %ld\n", dropX, dropY);
    int dropRow = table.getRowContainingPosition(dropX, dropY);

    return dropRow;
}

/**
 * Build the thing the target gets when something is dropped.
 * 
 * from the demo:
 * for our drag description, we'll just make a comma-separated list of the selected row
 * numbers - this will be picked up by the drag target and displayed in its box.
 *
 * In the context of MultiSelectDrag we want to move a set of strings from
 * one list box to another.  The easiest way to do that is to have the description
 * be array of strings.  A CSV is unreliable because an item in the array could contain
 * a comma, and I don't want to mess with delimiters and quoting.
 *
 * Passing just the item numbers like the demo means we have to ask some parent
 * component what those numbers mean.  This might make StringArrayListBox more usable
 * in different contexts, but more work.
 *
 * It is unclear what the side effects of having the description be an arbitrarily
 * long array of arbitrarily long strings would be.
 */
juce::var BindingTable::getDragSourceDescription (const juce::SparseSet<int>& selectedRows)
{
    juce::String desc;
    if (type == TypeButton) {
        if (selectedRows.size() > 1) {
            Trace(1, "BindingTable: Trying to drag more than one row");
        }
        else {
            desc = juce::String(selectedRows[0]);
        }
    }
    return desc;
}

/**
 * Finally after all that, we have our instructions.
 *
 * sourceRow is the row index you were ON when the drag started.
 * dropRow is the row you are on when the drag ended.
 *
 * The insertion line is painted at the top of the dropRow, indicating
 * that you want to the source row to be in between the dropRow and
 * the one above it.
 *
 * When dropRow == sourceRow you have not moved and nothing happens.
 *
 * When dropRow == sourceRow + 1 you are already above the drop row so
 * nothing happens.
 *
 * When dropRow is -1 it means that the drop happened outside of the
 * table rows so it moves to the end.  If sourceRow is already the last
 * one nothing happens.
 */
bool BindingTable::doMove(int sourceRow, int dropRow)
{
    bool moved = false;
    
    Trace(2, "BindingTable: Move row %d to %d", sourceRow, dropRow);

    if (dropRow < 0)
      // brain hurting, see comments in BindingTable for why this needs to be one past the end
      dropRow = bindingRows.size();

    if (sourceRow == dropRow) {
        // already there
    }
    else if (sourceRow == (dropRow - 1)) {
        // already above the target
    }
    else {
        // somewhere to go
        // SessionTrackTable where this came from fowarded this to the SessionEditor
        // we don't neeed to since we have the BindingSet container and can do it ourselves
        moveBinding(sourceRow, dropRow);
        moved = true;
    }
    return moved;
}

/**
 * The binding table would like to move a row.
 * This is active only when editing ButtonSets
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
        }
        
        reload();
        // keep on the same object
        // should already be there but make sure it's in sync
        selectRow(desiredRow);
        // we don't have this state
        //currentTrack = desiredRow;
        //show(currentTrack);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
