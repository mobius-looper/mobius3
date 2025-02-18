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
#include "model/MobiusConfig.h"
#include "model/Session.h"
#include "model/Preset.h"

#include "Provider.h"
#include "Producer.h"
#include "Symbolizer.h"

//////////////////////////////////////////////////////////////////////
//
// UISymbols
//
//////////////////////////////////////////////////////////////////////

// update: Don't like having these, you can just define everything with symbols.xml

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
    
    {SymbolIdNone, false, nullptr}
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

Symbolizer::Symbolizer(Provider* p)
{
    provider = p;
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

    // this is deferred until after the Sessions are initialized which
    // comes later in the Supervisor::start process
    //installActivationSymbols();

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
    SymbolTable* symbols = provider->getSymbols();

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
    SymbolTable* symbols = provider->getSymbols();
    
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
            s->treePath = "UI";
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
            s->treePath = "UI";

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
    juce::File root = provider->getRoot();
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
            xmlError("Parse error: %s\n", doc.getLastParseError());
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
        FunctionProperties* func = new FunctionProperties();
        func->global = root->getBoolAttribute("global");
        func->sustainable = root->getBoolAttribute("sustainable");
        func->longPressable = root->getBoolAttribute("longPressable");
        func->mayFocus = root->getBoolAttribute("mayFocus");
        func->mayConfirm = root->getBoolAttribute("mayConfirm");
        func->mayCancelMute = root->getBoolAttribute("mayCancelMute");
        func->argumentHelp = root->getStringAttribute("argumentHelp");
        func->sustainHelp = root->getStringAttribute("sustainHelp");
        func->mayQuantize = root->getBoolAttribute("mayQuantize");
        // todo: generalize this into a track type specifier, possibly a csv
        func->midiOnly = root->getBoolAttribute("midi");
        
        // todo: need mayFocus, mayConfirm, and mayMuteCancel in here too!

        Symbol* s = provider->getSymbols()->intern(name);
        s->functionProperties.reset(func);
        s->behavior = BehaviorFunction;

        // only set level if was specified in the XML
        // most functions have their levels set as a side effect during
        // core symbol installation
        juce::String levelValue = root->getStringAttribute("level");
        if (levelValue.length() > 0) {
            SymbolLevel level = parseLevel(levelValue);
            func->level = level;
            s->level = level;
        }

        parseTrackTypes(root, s);

        s->treePath = root->getStringAttribute("tree");
        s->treeInclude = root->getStringAttribute("treeInclude");
        s->hidden = root->getBoolAttribute("hidden");
        
        // Trace(2, "Symbolizer: Installed function %s\n", name.toUTF8());
    }
}

void Symbolizer::parseTrackTypes(juce::XmlElement* el, Symbol* s)
{
    juce::String trackTypes = el->getStringAttribute("trackTypes");
    if (trackTypes.length() > 0) {
        juce::StringArray types = juce::StringArray::fromTokens(trackTypes, ",", "");
        for (auto type : types) {
            if (type == "Audio") {
                s->trackTypes.add(TrackTypeAudio);
            }
            else if (type == "Midi") {
                s->trackTypes.add(TrackTypeMidi);
            }
            else {
                Trace(1, "Symbolizer: Unknown track type %s", type.toUTF8());
            }
        }
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
    else if (name == "session")
      scope = ScopeSession;
    else if (name == "sessionTrack")
      scope = ScopeSessionTrack;
    else if (name == "ui")
      scope = ScopeUI;
    else if (name == "sync")
      scope = ScopeSync;

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

        // enums commony won't have type="enum" so they default to TypeInt
        // enum-ness is implied by value a value list
        // most things look at the value list, but Parameterizer didn't
        if (props->values.size() > 0)
          props->type = TypeEnum;

        juce::String options = el->getStringAttribute("options");
        
        props->dynamic = options.contains("dynamic");
        props->zeroCenter = options.contains("zeroCenter");
        props->control = options.contains("control");
        props->transient = options.contains("transient");
        props->juceValues = options.contains("juceValues");
        props->noBinding = options.contains("noBinding");
        props->displayBase = el->getIntAttribute("displayBase");
        props->displayType = el->getStringAttribute("displayType");
        props->displayHelper = el->getStringAttribute("displayHelper");
        props->coreName = el->getStringAttribute("coreName");

        props->mayFocus = options.contains("mayFocus");
        props->mayResetRetain = options.contains("resetRetain");
        
        Symbol* s = provider->getSymbols()->intern(name);
        s->parameterProperties.reset(props);
                
        // this seems to be necessary for some things
        s->behavior = BehaviorParameter;
        // Supervisor whines if this isn't set for the newer parameters
        // that don't have coreParameters
        juce::String levelValue = el->getStringAttribute("level");
        if (levelValue.length() > 0) {
            SymbolLevel level = parseLevel(levelValue);
            s->level = level;
        }

        s->treePath = el->getStringAttribute("tree");
        s->treeInclude = el->getStringAttribute("treeInclude");
        
        parseTrackTypes(el, s);
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
    juce::File root = provider->getRoot();
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
    SymbolTable* symbols = provider->getSymbols();
    
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
            else if (pname == "quantized") {
                s->functionProperties->quantized = bvalue;
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
    
    SymbolTable* symbols = provider->getSymbols();
    for (auto symbol : symbols->getSymbols()) {
        if (symbol->functionProperties != nullptr) {
            if (symbol->functionProperties->focus)
              addProperty(xmlroot, symbol, "focus", "true");
            
            if (symbol->functionProperties->confirmation)
              addProperty(xmlroot, symbol, "confirmation", "true");
            
            if (symbol->functionProperties->muteCancel)
              addProperty(xmlroot, symbol, "muteCancel", "true");

            if (symbol->functionProperties->quantized)
              addProperty(xmlroot, symbol, "quantized", "true");
        }
        else if (symbol->parameterProperties != nullptr) {
            if (symbol->parameterProperties->focus)
              addProperty(xmlroot, symbol, "focus", "true");
            
            if (symbol->parameterProperties->resetRetain)
              addProperty(xmlroot, symbol, "resetRetain", "true");
        }
    }

    juce::String xml = xmlroot.toString();
    
    juce::File fileroot = provider->getRoot();
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
    SymbolTable* symbols = provider->getSymbols();
    
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

//////////////////////////////////////////////////////////////////////
//
// Structure Activations
//
//////////////////////////////////////////////////////////////////////

/**
 * Add BehaviorActivation symbols for the Setups and Presets.
 * 
 * Like Script/Sample symbols, we can't unintern once they're there
 * or else binding tables that point to them will break.  But we can
 * mark them hidden so they won't show up in the binding tables, and
 * unresolved ones can be highlighted.
 *
 * Not really happy with the symbol use here, we've got a prefixed name
 * to make them unique and they can't reliably point to anything since
 * the config objects can be deleted out from under it easily.
 *
 * There isn't a way to tell it was resolved other than it having
 * BehaviorActivation, could add ActivationProperties like we do for
 * other symbol types but there isn't anything to put into it yet.
 * We could put the structure ordinal there?  But this happens so rarely
 * a name lookup isn't that bad.
 */
void Symbolizer::installActivationSymbols()
{
    SymbolTable* symbols = provider->getSymbols();
    // hide existing activation symbols
    for (auto symbol : symbols->getSymbols()) {
        if (symbol->behavior == BehaviorActivation) {
            symbol->hidden = true;
        }
    }

    Preset* presets = provider->getPresets();
    while (presets != nullptr) {
        juce::String name = juce::String(Symbol::ActivationPrefixPreset) + presets->getName();
        Symbol* s = symbols->intern(name);
        s->behavior = BehaviorActivation;
        s->level = LevelCore;
        s->hidden = false;
        presets = presets->getNextPreset();
    }

    // unclear if we want Sessions to have activation symbols
    // this feeds into there being an "activeSession" parameter of type=Structure
    // with all the ugly support for structure symbols
    // you can get there just as well with a LoadSession UI function
    // that takes an argument name

    Producer* producer = provider->getProducer();
    juce::StringArray sessionNames;
    producer->getSessionNames(sessionNames);
    for (auto name : sessionNames) {
        juce::String symbolName = juce::String(Symbol::ActivationPrefixSession) + name;
        Symbol* s = symbols->intern(name);
        s->behavior = BehaviorActivation;
        s->level = LevelUI;
        s->hidden = false;
    }

    // while we're here, if this is in fact a popular way to do this, could do the
    // same for Layouts and ButtonSets
    // pick a style, any style...
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


