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

#include "../../script/MslResult.h"

#include "ScriptResultTable.h"

ScriptResultTable::ScriptResultTable(Supervisor* s)
{
    supervisor = s;
    setName("ScriptResultTable");

    initialize();

    addColumn("Name", ColumnName, 100);
    addColumn("Session", ColumnSession, 100);
    addColumn("Value", ColumnValue, 100);
    addColumn("Error", ColumnError, 100);
    
    addCommand("Refresh");
}

ScriptResultTable::~ScriptResultTable()
{
}

void ScriptResultTable::load()
{
    results.clear();

    MslEnvironment* env = supervisor->getMslEnvironment();
    MslResult* list = env->getResults();
    while (list != nullptr) {
        ScriptResultTableRow* row = new ScriptResultTableRow();
        
        row->name = juce::String(list->name);
        row->session = juce::String(list->sessionId);
        if (list->value != nullptr)
          row->value = juce::String(list->value->getString());

        // in theory there can be more than one of these
        if (list->errors != nullptr) {
            row->error = list->errors->details;
        }
        
        results.add(row);
        list = list->getNext();
    }
    
    // load the things and make the list
    updateContent();
}

void ScriptResultTable::clear()
{
    results.clear();
    updateContent();
}

//////////////////////////////////////////////////////////////////////
//
// TypicalTable Overrides
//
//////////////////////////////////////////////////////////////////////

int ScriptResultTable::getRowCount()
{
    return results.size();
}

juce::String ScriptResultTable::getCellText(int rowNumber, int columnId)
{
    juce::String cell;
    
    ScriptResultTableRow* row = results[rowNumber];

    if (columnId == ColumnName) {
        cell = row->name;
    }
    else if (columnId == ColumnSession) {
        cell = row->session;
    }
    else if (columnId == ColumnValue) {
        cell = row->value;
    }
    else if (columnId == ColumnError) {
        cell = row->error;
    }

    return cell;
}

void ScriptResultTable::doCommand(juce::String name)
{
    if (name == "Refresh")
      load();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

