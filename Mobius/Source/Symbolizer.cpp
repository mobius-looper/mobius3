/**
 * Utility class to handle loading of the non-core symbol table.
 *
 * This is evolving to replace a number of older things.
 *
 * todo: Move what is now in UISymbols in here, and/or replace that with
 * things defined in the symbols file.
 */

#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/Symbol.h"
#include "model/FunctionProperties.h"

#include "Supervisor.h"

#include "Symbolizer.h"

Symbolizer::Symbolizer()
{
}

Symbolizer::~Symbolizer()
{
}

/**
 * Read the standard symbols.xml file and install things in the symbol table.
 */
void Symbolizer::initialize()
{
    loadSymbolDefinitions();
}

void Symbolizer::loadSymbolDefinitions()
{
    juce::File root = Supervisor::Instance->getRoot();
    juce::File file = root.getChildFile("symbols.xml");
    if (!file.existsAsFile()) {
        Trace(1, "Symbolizer: Initialization file not found\n");
    }
    else {
        Trace(2, "Symbolizer: Reading symbol file %s\n", file.getFullPathName().toUTF8());
        juce::String xml = file.loadFileAsString();
        juce::XmlDocument doc(xml);
        std::unique_ptr<juce::XmlElement> docel = doc.getDocumentElement();
        if (docel == nullptr) {
            xmlError("Parse parse error: %s\n", doc.getLastParseError());
        }
        else if (!docel->hasTagName("Symbols")) {
            xmlError("Unexpected XML tag name: %s\n", docel->getTagName());
        }
        else {
            for (auto* el : docel->getChildIterator()) {
                if (el->hasTagName("Function")) {
                    parseFunction(el);
                }
            }
        }
    }
}

void Symbolizer::xmlError(const char* msg, juce::String arg)
{
    juce::String fullmsg ("Symbolizer: " + juce::String(msg));
    if (arg.length() == 0)
      Trace(1, fullmsg.toUTF8());
    else
      Trace(1, fullmsg.toUTF8(), arg.toUTF8());
}

void Symbolizer::parseFunction(juce::XmlElement* root)
{
    juce::String name = root->getStringAttribute("name");
    if (name.length() == 0) {
        xmlError("Function with no name", "");
    }
    else {
        SymbolLevel level = parseLevel(root->getStringAttribute("level"));
        
        FunctionProperties* func = new FunctionProperties();
        func->level = level;
        func->sustainable = root->getBoolAttribute("sustainable");
        func->argumentHelp = root->getStringAttribute("argumentHelp");

        Symbol* s = Symbols.intern(name);
        s->functionProperties.reset(func);
        // don't replace this yet, it will normally be set as a side effect of
        // instsalling core functions
        // s->level = level;

        // Trace(2, "Symbolizer: Installed function %s\n", name.toUTF8());
    }
}

/**
 * Parse an XML level name into a SymbolLevel enumeration value.
 */
SymbolLevel Symbolizer::parseLevel(juce::String lname)
{
    SymbolLevel slevel = LevelNone;
    
    if (lname == "UI" || lname == "ui")
      slevel = LevelUI;
    else if (lname == "shell")
      slevel = LevelShell;
    else if (lname == "kernel")
      slevel = LevelKernel;
    else if (lname == "core")
      slevel = LevelCore;

    return slevel;
}

