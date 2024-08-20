
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "ValueSet.h"

#include "MainConfig.h"

MainConfig::MainConfig()
{
}

MainConfig::~MainConfig()
{
}


/**
 * Return the global parameter set.
 * If this is being called from a fresh install we bootstrap one.
 */
ValueSet* MainConfig::getGlobals()
{
    ValueSet* globals = find("Global");
    if (globals == nullptr) {
        globals = new ValueSet();
        globals->name = juce::String("Global");
        parameterSets.add(globals);
    }
    return globals;
}

/**
 * Look up a value set by name.
 * todo: probably will need a HashMap
 */
ValueSet* MainConfig::find(juce::String name)
{
    ValueSet* found = nullptr;

    for (auto set : parameterSets) {
        if (set->name == name) {
            found = set;
            break;
        }
    }
    return found;
}

void MainConfig::add(ValueSet* set)
{
    parameterSets.add(set);
}

//////////////////////////////////////////////////////////////////////
//
// XML
//
//////////////////////////////////////////////////////////////////////

void MainConfig::parseXml(juce::String xml)
{
    juce::XmlDocument doc(xml);
    std::unique_ptr<juce::XmlElement> root = doc.getDocumentElement();
    if (root == nullptr) {
        xmlError("Parse parse error: %s\n", doc.getLastParseError());
    }
    else if (!root->hasTagName("MainConfig")) {
        xmlError("Unexpected XML tag name: %s\n", root->getTagName());
    }
    else {
        for (auto* el : root->getChildIterator()) {
            if (el->hasTagName(ValueSet::XmlElement)) {
                ValueSet* set = new ValueSet();
                set->parse(el);
                parameterSets.add(set);
            }
            else {
                Trace(1, "MainConfig: Invalid XML element %s", el->getTagName().toUTF8());
            }
        }
    }
}

void MainConfig::xmlError(const char* msg, juce::String arg)
{
    juce::String fullmsg ("MainConfig: " + juce::String(msg));
    if (arg.length() == 0)
      Trace(1, fullmsg.toUTF8());
    else
      Trace(1, fullmsg.toUTF8(), arg.toUTF8());
}

juce::String MainConfig::toXml()
{
    juce::XmlElement root ("MainConfig");

    for (auto set : parameterSets)
      set->render(&root);

    return root.toString();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
