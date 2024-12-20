/**
 * A table of script history statistics.
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

#include "ScriptStatisticsTable.h"

ScriptStatisticsTable::ScriptStatisticsTable(Supervisor* s)
{
    supervisor = s;
    setName("ScriptStatisticsTable");

    initialize();

    addColumn("Name", ColumnName, 200);
    addColumn("Runs", ColumnRuns, 100);
    addColumn("Errors", ColumnErrors, 100);
    
    addCommand("Refresh");
}

ScriptStatisticsTable::~ScriptStatisticsTable()
{
}

void ScriptStatisticsTable::load()
{
    stats.clear();
    
    MslEnvironment* env = supervisor->getMslEnvironment();
    juce::Array<MslLinkage*> links = env->getLinks();
    for (auto link : links) {
        if (link->isFunction) {
            ScriptStatisticsTableRow* row = new ScriptStatisticsTableRow();
        
            row->name = link->name;
            row->runs = link->runCount;
            row->errors = link->errorCount;

            stats.add(row);
        }
    }
    // load the things and make the list
    updateContent();
    // not always refrshing if it was already up
    repaint();
}

void ScriptStatisticsTable::clear()
{
    stats.clear();
    updateContent();
}

//////////////////////////////////////////////////////////////////////
//
// TypicalTable Overrides
//
//////////////////////////////////////////////////////////////////////

int ScriptStatisticsTable::getRowCount()
{
    return stats.size();
}

juce::String ScriptStatisticsTable::getCellText(int rowNumber, int columnId)
{
    juce::String cell;
    
    ScriptStatisticsTableRow* row = stats[rowNumber];

    if (columnId == ColumnName) {
        cell = row->name;
    }
    else if (columnId == ColumnRuns) {
        cell = juce::String(row->runs);
    }
    else if (columnId == ColumnErrors) {
        cell = juce::String(row->errors);
    }

    return cell;
}

void ScriptStatisticsTable::doCommand(juce::String name)
{
    if (name == "Refresh")
      load();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

