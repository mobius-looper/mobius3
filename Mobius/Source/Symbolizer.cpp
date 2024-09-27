/**
 * Utility class to handle loading of the non-core symbol table.
 *
 * This is evolving to replace a number of older things.
 */

#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/Symbol.h"
#include "model/SymbolId.h"
#include "model/FunctionProperties.h"
#include "model/UIParameter.h"
#include "model/ParameterProperties.h"

#include "Supervisor.h"
#include "Symbolizer.h"

//////////////////////////////////////////////////////////////////////
//
// UISymbols
//
//////////////////////////////////////////////////////////////////////

/**
 * Functions
 * format: id, public, signature
 */
UISymbols::Function UISymbols::Functions[] = {

    // public
    {FuncParameterUp, true, nullptr},
    {FuncParameterDown, true, nullptr},
    {FuncParameterInc, true, nullptr},
    {FuncParameterDec, true, nullptr},
    {FuncReloadScripts, true, nullptr},
    {FuncReloadSamples, true, nullptr},
    {FuncShowPanel, true, nullptr},
    {FuncMessage, true, nullptr},
    
    // scripts
    {FuncScriptAddButton, false, nullptr},
    {FuncScriptListen, false, nullptr},
    
    {SymbolIdNone, nullptr}
};

/**
 * Parameters
 * format: id, displayname
 */
UISymbols::Parameter UISymbols::Parameters[] = {

    // public
    {ParamActiveLayout, "Active Layout"},
    {ParamActiveButtons, "Active Buttons"},
    {ParamBindingOverlays, "Binding Overlays"},
    
    {SymbolIdNone, nullptr}
};

//////////////////////////////////////////////////////////////////////
//
// Symbolizer
//
//////////////////////////////////////////////////////////////////////

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
 * Interns symbols for the static SymbolDefinitions in model/SymbolId
 *
 * Adds Symbol annotations defined by static UISymbols above.  This style of using
 * static objects is what I would like to use for most simple Symbols that
 * don't need user tweaking of the definitions.
 *
 * Adds Symbol annotations defined by the static model/UIParameter and
 * model/FunctionDefinition objects.  This is the old model for definitions
 * that is being phased out.
 * 
 * Reads the symbols.xml file and adds symbol annotations for both functions
 * and parameters. This will eventually replace the static definition objects.
 * Externalizing the symbol definitions in a file allows the me or the user
 * to tweak definitions after installation.
 *
 * Reads the properties.xml file and adorns the Symbols with user-defined options.
 * These are not part of the symbol definition, they are more like user preferences
 * and have a UI so that users don't have to edit XML.  Still not entirely
 * happy with this.
 *
 * For the upgrader, and possibly others, it builds a lookup table for mapping
 * parameter id numbers to names.
 */
void Symbolizer::initialize()
{
    internSymbols();
    
    installUISymbols();

    loadSymbolDefinitions();
    
    loadSymbolProperties();

    // temporary for ConfigEditors, verify that the definitions match
    installOldDefinitions();
}

/**
 * Start the internment of symbols by iterating over the SymbolDefinition
 * objects defined in model/SymbolId.cpp.
 *
 * Also builds a table for quickly mapping ids to symbols.
 */
void Symbolizer::internSymbols()
{
    SymbolTable* symbols = supervisor->getSymbols();

    // I'm curious how large this is getting, be careful it doesn't grow
    // beyond 255 for optimized switch() compilation
    Trace(2, "Symbolizer: Interning %d standard symbols", (int)SymbolIdMax);

    for (int i = 0 ; SymbolDefinitions[i].name != nullptr ; i++) {
        SymbolDefinition* def = &(SymbolDefinitions[i]);
        Symbol* s = symbols->find(def->name);
        if (s != nullptr) {
            // should not have duplicates in this array
            // MobiusViewer and possibly some other member object constructors
            // intern symbols like "subcycles" for eventual queries before
            // Symbolizer has a chance to run, in those cases the id will be missing
            if (s->id != 0)
              Trace(1, "Symbolizer: Multiple definitions for symbol %s", def->name);
        }
        else {
            s = symbols->intern(def->name);
        }
        s->id = def->id;
    }

    // now that they are all there, build the id/name mapping table
    symbols->buildIdMap();
    
}

//////////////////////////////////////////////////////////////////////
//
// UI Symbols
//
//////////////////////////////////////////////////////////////////////

void Symbolizer::installUISymbols()
{
    SymbolTable* symbols = supervisor->getSymbols();
    
    for (int i = 0 ; UISymbols::Functions[i].id != SymbolIdNone ; i++) {
        UISymbols::Function* f = &(UISymbols::Functions[i]);
        
        Symbol* s = symbols->getSymbol(f->id);
        if (s == nullptr) {
            Trace(1, "Symbolizer: Missing SymbolDefinition for function %d", f->id);
        }
        else {
            s->behavior = BehaviorFunction;
            s->level = LevelUI;
            if (!f->visible)
              s->hidden = true;
            // todo: parse and store the signature
            // atm, these don't need full blown FunctionProperties annotations
            // Well, one of these "ReloadScripts" also has a core function which will
            // complain if it finds a symbol that doesn't have a property object
            // It's a good idea anyway
            FunctionProperties* props = new FunctionProperties();
            s->functionProperties.reset(props);
        }
    }
    
    for (int i = 0 ; UISymbols::Parameters[i].id != SymbolIdNone ; i++) {
        UISymbols::Parameter* p = &(UISymbols::Parameters[i]);
        
        Symbol* s = symbols->getSymbol(p->id);
        if (s == nullptr) {
            Trace(1, "Symbolizer: Missing SymbolDefinition for parameter %d", p->id);
        }
        else {
            s->behavior = BehaviorParameter;
            s->level = LevelUI;
            if (p->displayName == nullptr)
              s->hidden = true;

            // !! the assumption right now is that these are all TypeStructure
            // but that won't always be true, need more of a definition structure
            ParameterProperties* props = new ParameterProperties();
            if (p->displayName != nullptr)
              props->displayName = juce::String(p->displayName);
            props->type = TypeStructure;
            props->scope = ScopeUI;
            s->parameterProperties.reset(props);
        }
    }
}    

//////////////////////////////////////////////////////////////////////
//
// symbols.xml File Loading
//
//////////////////////////////////////////////////////////////////////

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
        func->global = root->getBoolAttribute("global");
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

        props->displayName = el->getStringAttribute("displayName");
        if (props->displayName.length() == 0)
          props->displayName = formatDisplayName(name);
        
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

        props->mayFocus = options.contains("mayFocus");
        props->mayResetRetain = options.contains("resetRetain");
        
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
            bool bvalue = isTruthy(value);
            if (pname == "focus") {
                s->parameterProperties->focus = bvalue;
            }
            else if (pname == "resetRetain") {
                s->parameterProperties->resetRetain = bvalue;
            }
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
        else if (symbol->parameterProperties != nullptr) {
            if (symbol->parameterProperties->focus)
              addProperty(xmlroot, symbol, "focus", "true");
            
            if (symbol->parameterProperties->resetRetain)
              addProperty(xmlroot, symbol, "resetRetain", "true");
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
// Old Static Definitions
//
//////////////////////////////////////////////////////////////////////

/**
 * Adorn Symbols using the old FunctionDefinition and UIParameter
 * static objects.  There should be complete overlap now with things
 * defined in the symbols.xml file.
 *
 * These do not use FunctionProperties and ParameterProperties, they
 * use the older convention of putting a pointer to the FunctionDefinition
 * and ParameterDefinition directly on the symbol.  When symbols.xml is loaded
 * the symbols will get properties objects that should match.
 *
 * Soon, this will all go away but we have to rewrite the ui/config classes
 * to use symbols instead of UIParameters.
 */
void Symbolizer::installOldDefinitions()
{
    SymbolTable* symbols = supervisor->getSymbols();
    
    // adorn Symbols from the older static FunctionDefinitions

    // don't know why this was here, Mobius will do this when it gets around
    // to initialization and I want to get rid of FunctionDefinition
#if 0    
    for (int i = 0 ; i < FunctionDefinition::Instances.size() ; i++) {
        FunctionDefinition* def = FunctionDefinition::Instances[i];
        Symbol* s = symbols->find(def->name);
        if (s == nullptr) {
            // something was missing in SymbolDefinitions
            Trace(1, "Symbolizer: Missing SymbolDefinition for %s", def->name);
        }
        else {
            if (s->functionProperties == nullptr) {
                // this must have been missing from symbols.xml
                Trace(1, "Symbolizer: Missing symbols.xml definition for %s", def->name);
            }
        
            s->behavior = BehaviorFunction;
            // start them out in core, Mobuis can change that
            s->level = LevelCore;
            // we have an ordinal but that won't be used any more
            s->function = def;
        }
    }
#endif    
    
    // adorn Symbols fromolder UIParameter definitions
    for (int i = 0 ; i < UIParameter::Instances.size() ; i++) {
        UIParameter* def = UIParameter::Instances[i];

        // if this is flagged deprecated, it is there only for the XmlRenderer and should
        // not be isntalled
        if (def->deprecated) continue;
        
        Symbol* s = symbols->find(def->name);
        if (s == nullptr) {
            Trace(1, "Symbolizer: Missing SymbolDefinition for %s", def->name);
        }
        else {
            if (s->parameterProperties == nullptr) {
                Trace(1, "Symbolizer: Missing symbols.xml definition for %s", def->name);
            }
            s->behavior = BehaviorParameter;
            s->level = LevelCore;
            s->parameter = def;
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


