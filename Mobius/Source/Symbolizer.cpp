/**
 * Utility class to handle loading of the non-core symbol table.
 *
 * This is evolving to replace a number of older things.
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
 * This does the following things:
 *
 * Interns symbols for the static FunctionDefinition and UIParameter objects.
 * 
 * Reads the symbols.xml file and interns function and parameter symbols and
 * fleshes out the FunctionProperties and ParameterProperties objects.
 * This will eventually replace the static definition objects.
 *
 * Interns a few UI symbols that are not defined in symbols.xml or with static
 * objects.
 *
 * Reads the properties.xml file and adorns the Symbols with user-defined options.
 */
void Symbolizer::initialize()
{
    // install the older static FunctionDefinitions
    for (int i = 0 ; i < FunctionDefinition::Instances.size() ; i++) {
        FunctionDefinition* def = FunctionDefinition::Instances[i];
        Symbol* s = supervisor->getSymbols()->intern(def->name);
        s->behavior = BehaviorFunction;
        // start them out in core, Mobuis can change that
        s->level = LevelCore;
        // we have an ordinal but that won't be used any more
        s->function = def;
    }

    // install older UIParameter definitions
    for (int i = 0 ; i < UIParameter::Instances.size() ; i++) {
        UIParameter* def = UIParameter::Instances[i];
        Symbol* s = supervisor->getSymbols()->intern(def->name);
        s->behavior = BehaviorParameter;
        s->level = LevelCore;
        s->parameter = def;
    }

    // load the new symbols.xml definition file
    loadSymbolDefinitions();
    
    // install newer symbols for UI functions and parameters
    installUISymbols();

    // install symbol properties
    loadSymbolProperties();
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
        func->mayFocus = root->getBoolAttribute("mayFocus");
        func->mayConfirm = root->getBoolAttribute("mayConfirm");
        func->mayCancelMute = root->getBoolAttribute("mayCancelMute");
        func->argumentHelp = root->getStringAttribute("argumentHelp");
        func->sustainHelp = root->getStringAttribute("sustainHelp");

        // todo: need mayFocus, mayConfirm, and mayMuteCancel in here too!

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

//////////////////////////////////////////////////////////////////////
//
// Properties
//
//////////////////////////////////////////////////////////////////////

/**
 * Load the properties.xml file and adorn symbols.
 * 
 * This could be more general than it is, it exists currently for the function flags
 * function flags like focus, confirmation, muteCancel that used to be Preset parameters.
 *
 * They could also go in symbols.xml but that has just the definitions and not user defined
 * properties and I like keeping them seperate.
 *
 * Not entirely happy with modeling these as symbol properties, could also think of
 * the symbol value being a "function" object with settable properties that would work
 * for other object symbols as well.  But this gets the problem solved until it moves
 * to something more general.
 */
void Symbolizer::loadSymbolProperties()
{
    juce::File root = supervisor->getRoot();
    juce::File file = root.getChildFile("properties.xml");
    if (!file.existsAsFile()) {
        Trace(1, "Symbolizer: properties.xml not found\n");
    }
    else {
        Trace(2, "Symbolizer: Reading symbol properties from %s\n", file.getFullPathName().toUTF8());
        juce::String xml = file.loadFileAsString();
        juce::XmlDocument doc(xml);
        std::unique_ptr<juce::XmlElement> docel = doc.getDocumentElement();
        if (docel == nullptr) {
            xmlError("Parse parse error: %s\n", doc.getLastParseError());
        }
        else if (!docel->hasTagName("Properties")) {
            xmlError("Unexpected XML tag name: %s\n", docel->getTagName());
        }
        else {
            for (auto* el : docel->getChildIterator()) {
                if (el->hasTagName("Property")) {
                    parseProperty(el);
                }
            }
        }
    }
}

/**
 * Parse a Property element and install things on FunctionProperties
 * or ParameterProperties.
 * This assumes that the symbols have already been installed
 */
void Symbolizer::parseProperty(juce::XmlElement* el)
{
    SymbolTable* symbols = supervisor->getSymbols();
    
    juce::String sname = el->getStringAttribute("symbol");
    juce::String pname = el->getStringAttribute("name");
    juce::String value = el->getStringAttribute("value");
    
    if (sname.length() == 0) {
        Trace(1, "Symbolizer: Property without symbol name\n");
    }
    else if (pname.length() == 0) {
        Trace(1, "Symbolizer: Property without property name\n");
    }
    else {
        Symbol* s = symbols->find(sname);
        if (s == nullptr) {
            Trace(1, "Symbolizer: Undefined symbol %s\n", sname.toUTF8());
        }
        else if (s->functionProperties != nullptr) {
            // todo: need name constants
            bool bvalue = isTruthy(value);
            if (pname == "focus") {
                s->functionProperties->focus = bvalue;
            }
            else if (pname == "confirmation") {
                s->functionProperties->confirmation = bvalue;
            }
            else if (pname == "muteCancel") {
                s->functionProperties->muteCancel = bvalue;
            }
            else {
                Trace(1, "Symbolizer: Undefined property name %s\n", pname.toUTF8());
            }
        }
        else if (s->parameterProperties != nullptr) {
            // here would be the place to do the "restore after reset" or whatever
            // that's called
        }
    }
}

/**
 * Extremely complex heuristic to determine what is truth.
 */
bool Symbolizer::isTruthy(juce::String value)
{
    return value.equalsIgnoreCase("true");
}

/**
 * Capture the values of function and parameter properties and write them
 * back to the properties.xml file.
 *
 * This works differently than mobius.xml and uiconfig.xml and is only updated
 * on exit, though I suppose we could update it after every interactive editing session.
 */
void Symbolizer::saveSymbolProperties()
{
    juce::XmlElement xmlroot ("Properties");
    
    SymbolTable* symbols = supervisor->getSymbols();
    for (auto symbol : symbols->getSymbols()) {
        if (symbol->functionProperties != nullptr) {
            if (symbol->functionProperties->focus)
              addProperty(xmlroot, symbol, "focus", "true");
            
            if (symbol->functionProperties->confirmation)
              addProperty(xmlroot, symbol, "confirmation", "true");
            
            if (symbol->functionProperties->muteCancel)
              addProperty(xmlroot, symbol, "muteCancel", "true");
        }
    }

    juce::String xml = xmlroot.toString();
    
    juce::File fileroot = supervisor->getRoot();
    juce::File file = fileroot.getChildFile("properties.xml");
    file.replaceWithText(xml);
}

void Symbolizer::addProperty(juce::XmlElement& root, Symbol* s, juce::String name, juce::String value)
{
    juce::XmlElement* prop = new juce::XmlElement("Property");
    prop->setAttribute("symbol", s->name);
    prop->setAttribute("name", name);
    prop->setAttribute("value", value);
    root.addChildElement(prop);
}

//////////////////////////////////////////////////////////////////////
//
// Display Symbols
//
//////////////////////////////////////////////////////////////////////

void Symbolizer::installUISymbols()
{
    installDisplayFunction("UIParameterUp", UISymbolParameterUp);
    installDisplayFunction("UIParameterDown", UISymbolParameterDown);
    installDisplayFunction("UIParameterInc", UISymbolParameterInc);
    installDisplayFunction("UIParameterDec", UISymbolParameterDec);
    installDisplayFunction("ReloadScripts", UISymbolReloadScripts);
    installDisplayFunction("ReloadSamples", UISymbolReloadSamples);
    installDisplayFunction("ShowPanel", UISymbolShowPanel);
    installDisplayFunction("Message", UISymbolMessage);
    
    // runtime parameter experiment
    // I'd like to be able to create parameters at runtime
    // without needing static definition objects

    installDisplayParameter(UISymbols::ActiveLayout, UISymbols::ActiveLayoutLabel, UISymbolActiveLayout);
    installDisplayParameter(UISymbols::ActiveButtons, UISymbols::ActiveButtonsLabel, UISymbolActiveButtons);
}    

/**
 * A display function only needs a symbol.
 */
void Symbolizer::installDisplayFunction(const char* name, int symbolId)
{
    Symbol* s = supervisor->getSymbols()->intern(name);
    s->behavior = BehaviorFunction;
    s->id = (unsigned char)symbolId;
    s->level = LevelUI;
}

/**
 * Runtime defined parameters are defined by two things,
 * a Symbol that reserves the name and a ParameterProperties that
 * defines the characteristics of the parameter.
 *
 * There is some confusing overlap on the Symbol->level and
 * ParameterProperties->scope.  As we move away from UIParameter/FunctionDefinition
 * to the new ParameterProperties and FunctionProperties need to rethink this.
 * ParameterProperties is derived from UIParameter where scopes include things
 * like global, preset, setup, and UI.  This is not the same as Symbol->level but
 * in the case of UI related things they're the same since there are no UI level
 * parameters with Preset scope for example.  So it looks like duplication
 * but it's kind of not.
 */
void Symbolizer::installDisplayParameter(const char* name, const char* label, int symbolId)
{
    ParameterProperties* p = new ParameterProperties();
    p->displayName = juce::String(label);
    p->type = TypeStructure;
    p->scope = ScopeUI;
    
    Symbol* s = supervisor->getSymbols()->intern(name);
    s->behavior = BehaviorParameter;
    s->id = (unsigned char)symbolId;
    s->level = LevelUI;
    s->parameterProperties.reset(p);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


