
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../model/Symbol.h"
#include "../../model/Session.h"
#include "../../Supervisor.h"
#include "../../Producer.h"

#include "../common/ButtonBar.h"

#include "SessionManagerTable.h"

SessionManagerTable::SessionManagerTable(Supervisor* s)
{
    supervisor = s;
    setName("SessionManagerTable");

    initialize();

    addColumn("Name", ColumnName, 200);
    
    addCommand("Refresh");
}

SessionManagerTable::~SessionManagerTable()
{
}

void SessionManagerTable::load()
{
    sessions.clear();

    Producer* producer = supervisor->getProducer();
    juce::StringArray names;
    producer->getSessionNames(names);

    for (auto name : names) {
        SessionManagerTableRow* row = new SessionManagerTableRow();
        row->name = name;
        sessions.add(row);
    }
    
    // load the things and make the list
    updateContent();
}

// this doesn't make sense right?
void SessionManagerTable::clear()
{
    //results.clear();
    updateContent();
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

void SessionManagerTable::doCommand(juce::String name)
{
    if (name == "Refresh")
      load();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

