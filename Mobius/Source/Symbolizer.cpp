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
#include "model/FunctionDefinition.h"
#include "model/FunctionProperties.h"
#include "model/UIParameter.h"
#include "model/ParameterProperties.h"

#include "Supervisor.h"

#include "Symbolizer.h"

Symbolizer::Symbolizer(Supervisor* s)
{
    supervisor = s;
}

Symbolizer::~Symbolizer()
{
}

/**
 * Read the standard symbols.xml file and install things in the symbol table.
 */
void Symbolizer::initialize()
{
    // install the older static FunctionDefinition and UIParameter symbols
    // these should have complete overlap with the new dynamically loaded symbols.xml definitions
    for (int i = 0 ; i < FunctionDefinition::Instances.size() ; i++) {
        FunctionDefinition* def = FunctionDefinition::Instances[i];
        Symbol* s = supervisor->getSymbols()->intern(def->name);
        s->behavior = BehaviorFunction;
        // start them out in core, Mobuis can change that
        s->level = LevelCore;
        // we have an ordinal but that won't be used any more
        s->function = def;
    }
        
    for (int i = 0 ; i < UIParameter::Instances.size() ; i++) {
        UIParameter* def = UIParameter::Instances[i];
        Symbol* s = supervisor->getSymbols()->intern(def->name);
        s->behavior = BehaviorParameter;
        s->level = LevelCore;
        s->parameter = def;
    }

    // the new symbol table definition file
    loadSymbolDefinitions();
}

void Symbolizer::loadSymbolDefinitions()
{
    juce::File root = supervisor->getRoot();
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
                else if (el->hasTagName("ParameterScope")) {
                    parseParameterScope(el);
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

//////////////////////////////////////////////////////////////////////
//
// Functions
//
//////////////////////////////////////////////////////////////////////

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
        func->sustainHelp = root->getStringAttribute("sustainHelp");

        Symbol* s = supervisor->getSymbols()->intern(name);
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

//////////////////////////////////////////////////////////////////////
//
// Parameters
//
//////////////////////////////////////////////////////////////////////

void Symbolizer::parseParameterScope(juce::XmlElement* el)
{
    juce::String scopeName = el->getStringAttribute("name");
    UIParameterScope scope = parseScope(scopeName);
    
    juce::XmlElement* child = el->getFirstChildElement();
    while (child != nullptr) {
        if (child->hasTagName("Parameter")) {
            parseParameter(child, scope);
        }
        child = child->getNextElement();
    }
}

/**
 * Parse an XML scope name into a UIParameterScope enumeration value.
 */
UIParameterScope Symbolizer::parseScope(juce::String name)
{
    UIParameterScope scope = ScopeGlobal;
    
    if (name == "global")
      scope = ScopeGlobal;
    else if (name == "preset")
      scope = ScopePreset;
    else if (name == "setup")
      scope = ScopeSetup;
    else if (name == "track")
      scope = ScopeTrack;
    else if (name == "ui")
      scope = ScopeUI;

    return scope;
}

/**
 * Parse an XML type name into a UIParameterType enumeration value.
 */
UIParameterType Symbolizer::parseType(juce::String name)
{
    UIParameterType type = TypeInt;
    
    if (name == "int")
      type = TypeInt;
    else if (name == "bool")
      type = TypeBool;
    else if (name == "enum")
      type = TypeEnum;
    else if (name == "string")
      type = TypeString;
    else if (name == "structure")
      type = TypeStructure;

    return type;
}

void Symbolizer::parseParameter(juce::XmlElement* el, UIParameterScope scope)
{
    juce::String name = el->getStringAttribute("name");
    if (name.length() == 0) {
        Trace(1, "Symbolizer: Parameter without name\n");
    }
    else {
        ParameterProperties* props = new ParameterProperties();
        props->scope = scope;
        props->type = parseType(el->getStringAttribute("type"));
        // todo: structureClass?
        props->multi = el->getBoolAttribute("multi");
        props->values = parseStringList(el->getStringAttribute("values"));
        props->valueLabels = parseLabels(el->getStringAttribute("valueLabels"), props->values);
        props->low = el->getIntAttribute("low");
        props->high = el->getIntAttribute("high");
        props->defaultValue = el->getIntAttribute("defaultValue");

        juce::String options = el->getStringAttribute("options");
        
        props->dynamic = options.contains("dynamic");
        props->zeroCenter = options.contains("zeroCenter");
        props->control = options.contains("control");
        props->transient = options.contains("transient");
        props->juceValues = options.contains("juceValues");
        props->noBinding = options.contains("noBinding");

        props->coreName = el->getStringAttribute("coreName");

        Symbol* s = supervisor->getSymbols()->intern(name);
        s->parameterProperties.reset(props);
    }
}

juce::StringArray Symbolizer::parseStringList(juce::String csv)
{
    return juce::StringArray::fromTokens(csv, ",", "");
}

juce::StringArray Symbolizer::parseLabels(juce::String csv, juce::StringArray values)
{
    juce::StringArray labels = juce::StringArray::fromTokens(csv, ",", "");
    if (labels.size() == 0) {
        for (int i = 0 ; i < values.size() ; i++) {
            labels.add(formatDisplayName(values[i]));
        }
    }
    return labels;
}

/**
 * Display name rules are initial capital followed by space
 * delimited words for each capital in the internal name.
 */
juce::String Symbolizer::formatDisplayName(juce::String xmlName)
{
    juce::String displayName;

    for (int i = 0 ; i < xmlName.length() ; i++) {
        juce::juce_wchar ch = xmlName[i];
        
        if (i == 0) {
            ch = juce::CharacterFunctions::toUpperCase(ch);
        }
        else if (juce::CharacterFunctions::isUpperCase(ch)) {
            displayName += " ";
        }
        displayName += ch;
    }

    return displayName;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


