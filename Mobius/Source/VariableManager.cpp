/**
 * Class encapsulating management of VariableDefinitions
 *
 * This is old now, and was an experiment that didn't get far.
 * Still potentially useful but needs thought.
 */

#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/Symbol.h"

#include "Supervisor.h"
#include "VariableManager.h"

VariableManager::VariableManager(Provider* p)
{
    provider = p;
}

VariableManager::~VariableManager()
{
}

void VariableManager::install()
{
    Trace(2, "VariableManager::install\n");

    // !! this needs to be using FileManasger

    juce::File root = provider->getRoot();
    juce::File file = root.getChildFile("variables.xml");
    if (!file.existsAsFile()) {
        Trace(2, "VariableManager: No variables.xml file\n");
    }
    else {
        juce::String xml = file.loadFileAsString();
        variables.parseXml(xml);

        bool traceit = false;
        if (traceit) {
            for (auto variable : variables.variables) {
                Trace(2, "  %s\n", variable->name.toUTF8());
            }

            // test XML serialization
            juce::String toxml = variables.toXml();
            TraceRaw("VariableDefinitionSet XML\n");
            TraceRaw(toxml.toUTF8());
            TraceRaw("\n");
        }

        // attach the VariableDefinitions to Symbols
        // if we ever support variable reloading this will
        // have to behave like Script symbols where we unresolve
        // some, update some, and add some
        for (auto variable : variables.variables) {
            Symbol* s = provider->getSymbols()->intern(variable->name.toUTF8());
            if (s->variable != nullptr) {
                // shouldn't be here yet
                Trace(1, "VariableManager: Replacing Symbol VariableDefinition\n");
            }
            s->variable = variable;
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

