/**
 * A table of track summaries
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../model/Symbol.h"
#include "../../model/Session.h"
#include "../../Provider.h"
#include "../MobiusView.h"

// unfortunately can't be that independent unless
// you add a complex Listener for all the callbacks
#include "SessionTrackEditor.h"

#include "SessionTrackTable.h"

SessionTrackTable::SessionTrackTable()
{
    setName("SessionTrackTable");

    addColumn("Track", ColumnName, 200);
    
    rowPopup.add("Add...", 1);
    rowPopup.add("Delete...", 2);
    rowPopup.add("Rename...", 3);
    rowPopup.add("Bulk...", 4);

    emptyPopup.add("Add...", 1);
    emptyPopup.add("Bulk...", 4);

    addAlert.setTitle("Add Track");
    addAlert.setMessage("Select the track type to add");
    addAlert.addButton("Audio");
    addAlert.addButton("Midi");
    addAlert.addButton("Cancel");
    
    deleteAlert.setTitle("Delete Track");
    deleteAlert.setSerious(true);
    deleteAlert.setMessage("Are you sure you want to delete this track?");
    deleteAlert.addButton("Delete");
    deleteAlert.addButton("Cancel");

    renameDialog.setTitle("Rename Track");
    renameDialog.addField(&newName);
    renameDialog.addButton("Rename");
    renameDialog.addButton("Cancel");

    bulkDialog.setTitle("Bulk Add/Remove Tracks");
    bulkDialog.setMessage("Enter the total number of tracks of each type you wish to have.");
    bulkDialog.setMessageHeight(40);
    bulkDialog.addField(&audioCount);
    bulkDialog.addField(&midiCount);
    bulkDialog.addButton("Modify");
    bulkDialog.addButton("Cancel");
    
    bulkConfirm.setTitle("Are you sure?");
    bulkConfirm.setSerious(true);
    bulkConfirm.setMessageHeight(100);
    bulkConfirm.addButton("Modify");
    bulkConfirm.addButton("Cancel");

    // add ourselves as a MouseListener to pick up clicks outside the rows
    table.addMouseListener(this, false);
}

SessionTrackTable::~SessionTrackTable()
{
}

void SessionTrackTable::initialize(Provider* p, SessionTrackEditor* e)
{
    // nothing to do during initialization, must
    // reload the table every time the editor is opened
    (void)p;
    editor = e;
    
    // it is vital you call this to get the header and other parts
    // of the table defined, or else it won't display
    TypicalTable::initialize();
}

void SessionTrackTable::load(Provider* p, Session* s)
{
    (void)p;
    session = s;
    reload();
}

void SessionTrackTable::reload()
{
    tracks.clear();

    int total = session->getTrackCount();
    for (int i = 0 ; i < total ; i++) {
        int number = i+1;
        Session::Track* t = session->getTrackByIndex(i);
        
        juce::String name = juce::String(number) + ":";
        if (t->type == Session::TypeMidi)
          name += "Midi";
        else
          name += "Audio";
        if (t->name.length() > 0)
          name += ":" + t->name;

        SessionTrackTableRow* row = new SessionTrackTableRow();
        
        row->name = name;
        row->midi = (t->type == Session::TypeMidi);
        
        tracks.add(row);
    }
    
    // load the things and make the list
    updateContent();
    repaint();
}

/**
 * Now that we're effectively editing the Session, it doesn't make
 * any sense to call clear().  It's more clearing the Session and
 * then asking the table to reload.
 */
void SessionTrackTable::clear()
{
    Trace(1, "SessionTrackTable::clear Who is calling this?");
    //tracks.clear();
    //updateContent();
}

bool SessionTrackTable::isMidi(int row)
{
    SessionTrackTableRow* trow = tracks[row];
    return trow->midi;
}

//////////////////////////////////////////////////////////////////////
//
// TypicalTable Overrides
//
//////////////////////////////////////////////////////////////////////

int SessionTrackTable::getRowCount()
{
    return tracks.size();
}

juce::String SessionTrackTable::getCellText(int rowNumber, int columnId)
{
    juce::String cell;
    
    SessionTrackTableRow* row = tracks[rowNumber];

    if (columnId == ColumnName) {
        cell = row->name;
    }

    return cell;
}

void SessionTrackTable::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
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
void SessionTrackTable::mouseDown(const juce::MouseEvent& event)
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

void SessionTrackTable::yanPopupSelected(int id)
{
    switch (id) {
        case 1: startAdd(); break;
        case 2: startDelete(); break;
        case 3: startRename(); break;
        case 4: startBulk(); break;
    }
}

void SessionTrackTable::startAdd()
{
    addAlert.show(getParentComponent());
}

void SessionTrackTable::startDelete()
{
    deleteAlert.show(getParentComponent());
}

void SessionTrackTable::startRename()
{
    newName.setValue("");
    renameDialog.show(getParentComponent());
}

void SessionTrackTable::countTracks()
{
    audioTracks = 0;
    midiTracks = 0;
    for (auto t : tracks) {
        if (t->midi)
          midiTracks++;
        else
          audioTracks++;
    }
}

void SessionTrackTable::startBulk()
{
    countTracks();
    audioCount.setValue(juce::String(audioTracks));
    midiCount.setValue(juce::String(midiTracks));
    
    bulkDialog.setMessage("Enter the total number of tracks of each type you wish to have.");
    
    bulkDialog.show(getParentComponent());
}

void SessionTrackTable::startBulkConfirm(int button)
{
    if (button == 0) {

        int newAudio = audioCount.getInt();
        int newMidi = midiCount.getInt();

        if (newAudio >= audioTracks && newMidi >= midiTracks) {
            finishBulk(0);
        }
        else {
            juce::String msg("You are deleting the highest ");
            if (newAudio < audioTracks) {
                msg += juce::String(audioTracks - newAudio);
                msg += juce::String(" audio tracks");
            }
            if (newMidi < midiTracks) {
                if (newAudio < audioTracks)
                  msg += " and ";
                msg += juce::String(midiTracks - newMidi);
                msg += juce::String(" midi tracks.");
            }
            else {
                msg += ".";
            }

            msg += "\nYou will lose all configuration and content for those tracks.\nThis cannot be undone.";
            
            bulkConfirm.setMessage(msg);
            bulkConfirm.show(getParentComponent());
        }
    }
}

void SessionTrackTable::yanDialogClosed(YanDialog* d, int button)
{
    if (d == &addAlert)
      finishAdd(button);
    else if (d == &deleteAlert)
      finishDelete(button);
    else if (d == &renameDialog)
      finishRename(button);
    else if (d == &bulkDialog)
      startBulkConfirm(button);
    else if (d == &bulkConfirm)
      finishBulk(button);
}

void SessionTrackTable::finishAdd(int button)
{
    if (button > 1) {
        // cancel
    }
    else {
        Session::TrackType type = ((button == 0) ? Session::TypeAudio : Session::TypeMidi);
        editor->addTrack(type);
    }
}

void SessionTrackTable::finishDelete(int button)
{
    if (button == 0) {
        editor->deleteTrack(getSelectedRow());
    }
}

void SessionTrackTable::finishRename(int button)
{
    if (button == 0) {
        int row = getSelectedRow();
        Session::Track* t = session->getTrackByIndex(row);
        // todo: should have some validation on allowed names
        t->name = newName.getValue();
        reload();
    }
}

void SessionTrackTable::finishBulk(int button)
{
    if (button == 0) {
        editor->bulkReconcile(audioCount.getInt(), midiCount.getInt());
    }
}

//////////////////////////////////////////////////////////////////////
//
// Reorder Hacking
//
//////////////////////////////////////////////////////////////////////

bool SessionTrackTable::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)details;
    //Trace(2, "SessionTrackTable::isInterestedInDragSource");
    return true;
}

void SessionTrackTable::itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details)
{
    // we are both a source and a target, so don't highlight if we're over oursleves
    // spec is unclear what the sourceComponent will be if this is an item from
    // a ListBox, what you are dragging is some sort of inner component for the ListBox
    // with arbitrary structure between it and the ListBox, comparing against the
    // outer ListBox seems to work
    if (details.sourceComponent != &table) {
        Trace(2, "SessionTrackTable::itemDragEnter From outside");
        targetActive = true;
        moveActive = false;
    }
    else {
        //Trace(2, "SessionTrackTable::itemDragEnter From inside");
        // moving within ourselves
        moveActive = true;
        targetActive = false;
    }
    paintDropTarget = true;
}

/**
 * If we're dragging within ourselves, give some indication of the insertion point.
 * Actually it doesn't matter if the drag is coming from the outside, still need
 * to be order sensitive unless sorted.  I gave up trying to predict what
 * getInsertionIndexForPosition does.  You can calculate the drop position without that
 * in itemDropped, though it would be nice to draw that usual insertion line between items
 * while the drag is in progress.  Revisit someday...
 */
void SessionTrackTable::itemDragMove(const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::Point<int> pos = details.localPosition;
    // position is "relative to the target component"  in this case the target
    // is the SessionTrackTable which offsets the ListBox by 4 on all sides to draw
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

/**
 * If we started a drag, and went off into space without landing on a target, I suppose we
 * could treat this as a special form of move that removes the value from the list.
 * But I don't think we can tell from here, this just means that the mouse left the
 * ListBox, it may come back again.
 */
void SessionTrackTable::itemDragExit (const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)details;
    Trace(2, "SessionTrackTable::itemDragExit");
    targetActive = false;
    moveActive = false;
    paintDropTarget = false;
    dropTargetRow = -1;
    repaint();
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
void SessionTrackTable::itemDropped (const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)details;

    // here is where you would figure out the source row by comparing what
    // was placed in the details by getDragSourceDesription to one of the rows in the model
    //int sourceRow = strings.indexOf(newValues[0]);
    
    int dropRow = getDropRow(details);

    juce::String sourceThing = "???";;
    juce::var stuff = details.description;
    if (stuff.isArray()) {
        Trace(1, "SessionTrackTable: Something dropped in an array");
    }
    else {
        sourceThing = stuff;
    }
    
#if 0    
    juce::String msg = juce::String("Dropped ") + sourceThing + juce::String(" into row ") +
        juce::String(dropRow);
    Trace(2, "SessionTrackTable: %s", msg.toUTF8());
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

/**
 * Calculate the row where a drop should be inserted when using
 * an unordered list.
 *
 * getInsertionIndexForPosition tracking during itemDragMove was wonky
 * and I never did understand it.  We don't really need that since we have
 * the drop coordinates in details.localPosition and can ask the ListBox
 * for getRowContainingPosition.  Note that localPosition is relative to the
 * DragAndDropTarget which is SessionTrackTable and the ListBox is inset by
 * 4 on all sides to draw a border.  So have to adjust the coordinates to ListBox
 * coordinates when calling getRowCintainingPosition.
 */
int SessionTrackTable::getDropRow(const juce::DragAndDropTarget::SourceDetails& details)
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
juce::var SessionTrackTable::getDragSourceDescription (const juce::SparseSet<int>& selectedRows)
{
    juce::String desc;
    
    if (selectedRows.size() > 1) {
        Trace(1, "SessionTrackTable: Trying to drag more than one row");
    }
    else {
        desc = juce::String(selectedRows[0]);
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
bool SessionTrackTable::doMove(int sourceRow, int dropRow)
{
    bool moved = false;
    
    Trace(2, "SessionTrackTable: Move row %d to %d", sourceRow, dropRow);

    if (dropRow < 0)
      // brain hurting, see comments in SessionTrackTable for why this needs to be one past the end
      dropRow = session->getTrackCount();

    if (sourceRow == dropRow) {
        // already there
    }
    else if (sourceRow == (dropRow - 1)) {
        // already above the target
    }
    else {
        // somewhere to go
        editor->move(sourceRow, dropRow);
        moved = true;
    }
    return moved;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

