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
 * It should be possible for this to work using only the Session
 * but that is sparse right now.  To know track names you need information
 * from the Setup which will have been combined into the MobiusView.
 * This also makes the order of the tracks match the View order, which
 * SHOULD also match Session order, but since that's what drives the UI, use it.
 *
 * The session passed here is the one being edited by SessionEditor
 * which one day may be complete enough to drive the process but for now
 * is ignored.  
 */
void SessionTrackTable::load(Provider* p, Session* session)
{
    (void)session;

    tracks.clear();

    // the Session unfortunately can't drive the table because
    // audio track configurations are not kept in here
    // the MobiusView is better because the order also corresponds to
    // the canonical track numbers

    // todo: need to start saving internal track numbers on the Session so
    // we go between them quickly, is that working?

    //Session* session = provider->getSession();
    MobiusView* view = p->getMobiusView();

    // the view will have display order eventually which is probably a good
    // way to order the table rows, but if we show the internal numbers it may
    // be confusing to see them out of order to maybe reorder them
    // SystemState is not accessible here

    for (int i = 0 ; i < view->totalTracks ; i++) {
        MobiusViewTrack* t = view->getTrack(i);

        // !! put the fucking number in the view instead of the index
        int number = t->index + 1;

        // experiment here...
        juce::String name = juce::String(number) + ":";
        if (t->midi)
          name += "Midi";
        else
          name += "Audio";
        if (t->name.length() > 0)
          name += ":" + t->name;

        SessionTrackTableRow* row = new SessionTrackTableRow();
        
        row->name = name;
        row->number = number;
        row->midi = t->midi;
        
        tracks.add(row);
    }
    
    // load the things and make the list
    updateContent();
    repaint();
}

void SessionTrackTable::clear()
{
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
// Menu Handlers
//
//////////////////////////////////////////////////////////////////////

void SessionTrackTable::yanPopupSelected(int id)
{
    switch (id) {
        case 1: menuAdd(); break;
        case 2: menuDelete(); break;
        case 3: menuRename(); break;
        case 4: menuBulk(); break;
    }
}

void SessionTrackTable::menuAdd()
{
    addDialog.setTitle("Add Track");
    addDialog.setMessage("Select the track type to add");
    addDialog.addButton("Cancel");
    addDialog.addButton("Audio");
    addDialog.addButton("Midi");
    addDialog.show();
}

void SessionTrackTable::menuDelete()
{
    deleteDialog.setTitle("Delete Track");
    deleteDialog.setSerious(true);
    deleteDialog.setMessage("Are you sure you want to delete this track?\nDeleting a track will reunmber the tracks and may cause instability in the bindings if you used track numbers as scopes.");
    deleteDialog.addButton("Cancel");
    deleteDialog.addButton("Delete");
    deleteDialog.show();
}

void SessionTrackTable::menuRename()
{
}

void SessionTrackTable::menuBulk()
{
}

void SessionTrackTable::yanDialogSelected(YanDialog* d, int button)
{
    if (d == &deleteDialog) {
        if (button == 0)
          Trace(2, "SessionTrackTable: No, I'm scared");
        else
          Trace(2, "SessionTrackTable: Yes, I'm brave");
    }
    else if (d == &addDialog) {
        if (button == 0)
          Trace(2, "SessionTrackTable: Forget the add");
        else if (button == 1)
          Trace(2, "SessionTrackTable: Adding an audio");
        else if (button == 2)
          Trace(2, "SessionTrackTable: Forget a Midi");
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

