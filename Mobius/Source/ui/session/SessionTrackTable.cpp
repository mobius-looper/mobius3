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

    addColumn("Name", ColumnName, 200);
    
    addCommand("Refresh");
}

SessionTrackTable::~SessionTrackTable()
{
}
void SessionTrackTable::initialize(Provider* p)
{
    // nothing to do during initialization, must
    // reload the table every time the editor is opened
    (void)p;
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
    return row;

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

void SessionTrackTable::doCommand(juce::String name)
{
    (void)name;
    //if (name == "Refresh")
    //load();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

