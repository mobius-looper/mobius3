/**
 * A table of running script processes.
 * This is almost always empty or small
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../model/Symbol.h"
#include "../../model/ScriptProperties.h"
#include "../../Supervisor.h"
#include "../../Prompter.h"

#include "../common/ButtonBar.h"
#include "../JuceUtil.h"
#include "../MainWindow.h"

#include "../../script/ScriptRegistry.h"
#include "../../script/MslDetails.h"
#include "../../script/MslLinkage.h"
#include "../../script/MslCompilation.h"

#include "ScriptProcessTable.h"

ScriptProcessTable::ScriptProcessTable(Supervisor* s)
{
    supervisor = s;
    setName("ScriptProcessTable");

    initialize();

    addColumn("Name", ColumnName, 100);
    addColumn("Status", ColumnStatus, 100);
    addColumn("Session", ColumnSession, 100);
    
    addCommand("Refresh");
}

ScriptProcessTable::~ScriptProcessTable()
{
}

void ScriptProcessTable::load()
{
    processes.clear();
    
    MslEnvironment* env = supervisor->getMslEnvironment();
    juce::Array<MslProcess> result;
    env->listProcesses(result);
    
    for (int i = 0 ; i < result.size() ; i++) {
        MslProcess& p = result.getReference(i);
        ScriptProcessTableRow* row = new ScriptProcessTableRow();
        
        row->name = p.name;

        juce::String status;
        switch (p.state) {
            case MslStateNone: status = " no status"; break;
            case MslStateFinished: status = " finished"; break;
            case MslStateError: status = " errors"; break;
            case MslStateRunning: status = " running"; break;
            case MslStateWaiting: status = " waiting"; break;
            case MslStateSuspended: status = " suspended"; break;
            case MslStateTransitioning: status = " transitioning"; break;
        }
        row->status = status;
        
        row->session = juce::String(p.sessionId);

        processes.add(row);
    }
    
    // load the things and make the list
    updateContent();
}

void ScriptProcessTable::clear()
{
    processes.clear();
    updateContent();
}

//////////////////////////////////////////////////////////////////////
//
// TypicalTable Overrides
//
//////////////////////////////////////////////////////////////////////

int ScriptProcessTable::getRowCount()
{
    return processes.size();
}

juce::String ScriptProcessTable::getCellText(int rowNumber, int columnId)
{
    juce::String cell;
    
    ScriptProcessTableRow* row = processes[rowNumber];

    if (columnId == ColumnName) {
        cell = row->name;
    }
    else if (columnId == ColumnStatus) {
        cell = row->status;
    }
    else if (columnId == ColumnSession) {
        cell = row->session;
    }

    return cell;
}

void ScriptProcessTable::doCommand(juce::String name)
{
    if (name == "Refresh")
      load();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

