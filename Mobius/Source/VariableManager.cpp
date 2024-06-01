/**
 * Class encapsulating management of VariableDefinitions
 */

#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/Symbol.h"

#include "Supervisor.h"
#include "VariableManager.h"

VariableManager::VariableManager(Supervisor* super)
{
    supervisor = super;
}

VariableManager::~VariableManager()
{
}

void VariableManager::install()
{
    Trace(2, "VariableManager::install\n");

    juce::File root = supervisor->getRoot();
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
            Symbol* s = Symbols.intern(variable->name.toUTF8());
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

