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

#include "SessionTrackTable.h"

SessionTrackTable::SessionTrackTable()
{
    setName("SessionTrackTable");

    addColumn("Track", ColumnName, 200);
    
    popup.add("Add...", 1);
    popup.add("Delete...", 2);
    popup.add("Rename...", 3);
    popup.add("Bulk...", 4);

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
}

SessionTrackTable::~SessionTrackTable()
{
}

void SessionTrackTable::initialize(Provider* p)
{
    // nothing to do during initialization, must
    // reload the table every time the editor is opened
    (void)p;
    
    // it is vital you call this to get the header and other parts
    // of the table defined, or else it won't display
    TypicalTable::initialize();
}

/**
 * The order of tracks in the table will follow the internal track numbers.
 * It would be better to follow the view order once that becomes possible.
 *
 * The session must be normalized at this point.
 *
 */
void SessionTrackTable::load(Provider* p, Session* s)
{
    (void)p;
    session = s;
    reload();
}

void SessionTrackTable::reload()
{
    tracks.clear();

    // although the session is normalized to have the correct number
    // of Session::Track objects they may appear in random order.
    
    int total = session->getTrackCount();
    for (int i = 0 ; i < total ; i++) {
        // look up by internal number for ordering
        int number = i + 1;

        Session::Track* t = session->getTrackByNumber(number);
        
        juce::String name = juce::String(number) + ":";
        if (t->type == Session::TypeMidi)
          name += "Midi";
        else
          name += "Audio";
        if (t->name.length() > 0)
          name += ":" + t->name;

        SessionTrackTableRow* row = new SessionTrackTableRow();
        
        row->name = name;
        row->number = number;
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
    tracks.clear();
    updateContent();
}

int SessionTrackTable::getTrackNumber(int row)
{
    int number = 0;
    SessionTrackTableRow* trow = tracks[row];
    number = trow->number;
    return number;
}

int SessionTrackTable::getSelectedTrackNumber()
{
    int number = 0;
    int row = getSelectedRow();
    if (row >= 0)
      number = getTrackNumber(row);
    return number;
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
      popup.show();
    else
      TypicalTable::cellClicked(rowNumber, columnId, event);
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
        countTracks();
        // this will be the internal track number of the new track
        int newSelection = 0;
        if (button == 0) {
            session->reconcileTrackCount(Session::TypeAudio, audioTracks + 1);
            newSelection = audioTracks + 1;
        }
        else {
            session->reconcileTrackCount(Session::TypeMidi, midiTracks + 1);
            newSelection = audioTracks + midiTracks + 1;
        }
        reload();
        selectRowByNumber(newSelection);
    }
}

/**
 * After making structural modifications to the track list, pick
 * a new track to be highlighted and tell the listeners to refresh
 * whatever is following that.
 */
void SessionTrackTable::selectRowByNumber(int number)
{
    // we're currently keeping these in order
    int row = number - 1;
    selectRow(row);
    if (listener != nullptr)
      listener->typicalTableChanged(this, row);
}

void SessionTrackTable::finishDelete(int button)
{
    if (button == 0) {
        int number = getSelectedTrackNumber();
        session->deleteByNumber(number);
        reload();
        // go back to the beginning, though could try to be one after the
        // deleted one
        selectRowByNumber(0);
    }
}

void SessionTrackTable::finishRename(int button)
{
    if (button == 0) {
        int number = getSelectedTrackNumber();
        Session::Track* t = session->getTrackByNumber(number);
        // todo: should have some validation on allowed names
        t->name = newName.getValue();
        reload();
    }
}

/**
 * You can't define display order in this interface yet.
 * Tracks will be clustered by type and assigned numbers.
 */
void SessionTrackTable::finishBulk(int button)
{
    if (button == 0) {
        session->reconcileTrackCount(Session::TypeAudio, audioCount.getInt());
        session->reconcileTrackCount(Session::TypeMidi, midiCount.getInt());
        reload();
        selectRowByNumber(0);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

