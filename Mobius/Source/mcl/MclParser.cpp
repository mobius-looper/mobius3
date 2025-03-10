
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../util/Util.h"
#include "../model/Symbol.h"
#include "../model/ParameterProperties.h"
#include "../model/ParameterSets.h"
#include "../model/ValueSet.h"
#include "../model/old/Binding.h"
#include "../Provider.h"
#include "../Producer.h"

#include "MclModel.h"
#include "MclResult.h"
#include "MclParser.h"

MclParser::MclParser(Provider* p)
{
    provider = p;
}

MclParser::~MclParser()
{
}

MclScript* MclParser::parse(juce::String src, MclResult& userResult)
{
    script.reset(new MclScript());
    result = &userResult;

    juce::StringArray lines = juce::StringArray::fromLines(src);
    lineNumber = 1;
    line = "";
    for (int i = 0 ; i < lines.size() ; i++) {
        line = lines[i];
        parseLine();
        if (result->hasErrors())
          break;
        lineNumber++;
    }

    if (result->hasErrors()) {
        script.reset();
    }
    return script.release();
}

void MclParser::addError(juce::String err)
{
    result->errors.add(juce::String("Line ") + juce::String(lineNumber) + ": " + line);
    result->errors.add(err);
}

bool MclParser::hasErrors()
{
    return result->errors.size() > 0;
}

MclSection* MclParser::getSection()
{
    if (currentSection == nullptr) {
        // defaults to Session
        // hmm no, now that there can be several things in here
        // it causes confusing error messages if you do things
        // without any section context?
        MclSection* obj = new MclSection();
        script->add(obj);
        currentSection = obj;
    }
    return currentSection;
}
        
MclScope* MclParser::getScope()
{
    if (currentScope == nullptr) {
        MclSection* obj = getSection();
        // todo: verify that this object supports subscopes
        MclScope* scope = new MclScope();
        obj->add(scope);
        currentScope = scope;
    }
    return currentScope;
}

/**
 * Things to ponder...
 * String literals are messy.  You have to quote them which is reasonably
 * well understood and easy to deal with for assignment values but for track
 * names it's harder if you allow them as a line scope.
 *
 *      foo bar:subcycles 4
 *
 * It's easy enouugh to require this
 *
 *      "foo bar":subcycles 4
 *
 * But it's likely to be a common error, especially when you allow group names
 * as line scopes.  They think that : divides the two worlds when it actually doesn't
 * it's just converted to a space which will make it parse all wonky with "too many tokens".
 * Should just look for : and deal with each side individually.
 *
 * It's also likely with assignments to want to do this:
 *
 *    trackName=Foo Bar
 *
 * Which looks like it would work, and since assignments can't have extra keywords
 * following the value it might be expected to treat everything after the = or
 * space between the parameter name as a string.
 *
 * This works to quote single words:
 *
 *     trackname ""foo""
 *
 * But this doesn't because the tokenizer breaks on all spaces and the quoting
 * handled by StringArray isn't doing what I expected.
 *
 *    trackName ""foo bar""
 *
 * Not a big deal since users don't really need scare quotes around track names,
 * but I love irony so fix it.
 */
void MclParser::parseLine()
{
    // it is extremely common to write foo=bar like MSL scripts
    // do rather than "foo bar" without the equals
    // make tokenizing easier by just converting them to spaces
    // actually I like this approach for : scope prefixes as well
    // since you're not actually parsing the :
    juce::String rline = line.replaceCharacters("=:", "  ");
    juce::StringArray tokens = juce::StringArray::fromTokens(rline, " ", "\"");

    bool traceit = false;
    if (traceit) {
        Trace(2, "MclParser line %s", line.toUTF8());
        for (auto t : tokens)
          Trace(2, "  %s", t.toUTF8());
    }

    if (tokens.size() > 0) {

        juce::String keyword = tokens[0];
        if (keyword == MclSection::KeywordSession)
          parseSession(tokens);

        else if (keyword == MclSection::KeywordOverlay)
          parseOverlay(tokens);

        else if (keyword == MclSection::KeywordBinding ||
                 keyword == MclSection::KeywordBindings)
          parseBinding(tokens);

        else {
            MclSection* sec = getSection();
            if (sec->type == MclSection::Session)
              parseSessionLine(tokens, false);
            
            else if (sec->type == MclSection::Overlay)
              parseSessionLine(tokens, true);

            else if (sec->type == MclSection::Binding)
              parseBindingLine(tokens);

            else
              addError("Unhandled setion line");
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Sessions and Overlays
//
//////////////////////////////////////////////////////////////////////

void MclParser::parseSession(juce::StringArray& tokens)
{
    if (tokens.size() > 2) {
        addError("Too many Session section tokens");
    }
    else {
        MclSection* obj = new MclSection();
        script->add(obj);
        currentSection = obj;
        
        // session name is not required, it defaults to "active"
        if (tokens.size() > 1) {
            juce::String sname = tokens[1];
            if (sname == MclSection::NameActive) {
                // leave name blank
            }
            else if (sname == MclSection::EvalPermanent) {
                // the default
            }
            else if (sname == MclSection::EvalMemory)
              obj->duration = MclSection::Memory;
            else if (sname == MclSection::EvalTemporary)
              obj->duration = MclSection::Temporary;
            
            else {
                Producer* pro = provider->getProducer();
                juce::StringArray names;
                pro->getSessionNames(names);
                if (names.contains(sname))
                  obj->name = sname;
                else {
                    // a new session
                    // todo: verify name goodness
                    obj->name = tokens[1];
                }
            }
        }
    }
}

void MclParser::parseOverlay(juce::StringArray& tokens)
{
    if (tokens.size() < 2) {
        addError("Missing overlay name");
    }
    else if (tokens.size() > 3) {
        addError("Overlay section extra tokens");
    }
    else {
        juce::String oname = tokens[1];
        juce::String qualifier;
        if (tokens.size() == 3)
          qualifier = tokens[2];
        MclSection::Duration duration = MclSection::Permanent;

        if (qualifier == MclSection::EvalMemory)
          duration = MclSection::Memory;
        else if (qualifier == MclSection::EvalTemporary)
          duration = MclSection::Temporary;
        else if (qualifier != MclSection::EvalPermanent)
          addError(juce::String("Invalid overlay duration: ") + qualifier);

        if (!hasErrors()) {
            MclSection* obj = new MclSection();
            obj->type = MclSection::Overlay;
            obj->name = oname;
            obj->duration = duration;
            script->add(obj);
            currentSection = obj;
        }
    }
}

void MclParser::parseSessionLine(juce::StringArray& tokens, bool isOverlay)
{
    if (tokens.size() > 0) {
        juce::String first = tokens[0];
        if (first == MclScope::Keyword) {
            if (isOverlay) {
                // scopes not possibe in overlays yet
                addError("Scopes not allowed in Overlay section");
            }
            else {
                parseScope(tokens);
            }
        }
        else {
            parseAssignment(tokens);
        }
    }
}

void MclParser::parseScope(juce::StringArray& tokens)
{
    if (tokens.size() != 2) {
        addError("Missing scope identifier");
    }
    else {
        juce::String scopeId = tokens[1];
        int trackNumber = parseScopeId(scopeId);
        if (!hasErrors()) {
            MclScope* s = new MclScope();
            s->scopeId = scopeId;
            s->scope = trackNumber;
            MclSection* section = getSection();
            section->add(s);
            currentScope = s;
        }
    }
}

int MclParser::parseScopeId(juce::String token)
{
    int trackNumber = 0;

    // once these are allowed to be track names will need to trim quotes
    juce::String trackId = token.unquoted();
    
    // not supporting track names right now, must be a number
    if (!IsInteger(token.toUTF8())) {
        addError("Scope identifier not a track number");
    }
    else {
        trackNumber = ToInt(token.toUTF8());
    }
    return trackNumber;
}

void MclParser::parseAssignment(juce::StringArray& tokens)
{
    juce::String lineScope;
    juce::String sname;
    juce::String svalue;

    if (tokens.size() == 1) {
        addError("Missing tokens");
    }
    else if (tokens.size() == 2) {
        sname = tokens[0];
        svalue = tokens[1];
    }
    else if (tokens.size() == 3) {
        lineScope = tokens[0];
        sname = tokens[1];
        svalue = tokens[2];
    }
    else {
        addError("Too many tokens");
    }

    if (!hasErrors()) {
        int trackNumber = 0;
        if (lineScope.length() > 0)
          trackNumber = parseScopeId(lineScope);

        if (!hasErrors()) {

            bool isRemove = false;
            if (sname == "remove") {
                // special keyword for unassignment, works only in track scope overrides
                isRemove = true;
                sname = svalue;
                svalue = "";
            }
            else {
                // for the few parameters that are TypeString the "tokenizer" will have
                // left the surrounding quotes, remove them
                if (svalue.isQuotedString())
                  svalue = svalue.unquoted();
            }
            
            Symbol* s = provider->getSymbols()->find(sname);
            if (s == nullptr) {
                addError("Unknown symbol " + sname);
            }
            else {
                ParameterProperties* props = s->parameterProperties.get();
                if (props == nullptr) {
                    addError(juce::String("Symbol is not a parameter: " + s->name));
                }
                else {

                    // detect some nonsensical usages
                    MclScope* rscope = getScope();
                    if (!hasErrors()) {
                        // what about LevelShell?
                        if (s->level == LevelUI)
                          addError(juce::String("UI parameter cannot be set this way: ") + sname);
                        else if (s->treePath.length() == 0) {
                            // this is a bad way of detecting this but if a symbol doesn't
                            // have a treePath then it is considered global and won't be displayed
                            // in the parameter trees in the session/overlay editor
                            // if this symbol is used in mcl track scope, it is wrong since tracks
                            // can't overrode transportTempo or things like that
                            // would be better to be explicit about this with a scope='global'
                            // or level='track' but those aren't being defined accurately right now
                            if (rscope->scope > 0 || trackNumber > 0)
                              addError(juce::String("Global parameter cannot have a track override: ") + sname);
                        }
                    }

                    int ordinal = 0;
                    if (!hasErrors()) {
                        if (!isRemove) {
                            if (props->type == TypeStructure)
                              validateStructureReference(s, props, svalue);
                    
                            else if (props->type != TypeString)
                              ordinal = parseParameterOrdinal(s, props, svalue, isRemove);
                        }
                    }
                    
                    if (!hasErrors()) {
                        MclAssignment* ass = new MclAssignment();
                        ass->name = sname;
                        ass->symbol = s;
                        ass->svalue = svalue;
                        ass->remove = isRemove;
                        ass->scopeId = lineScope;
                        ass->scope = trackNumber;

                        ass->value.setNull();
                        if (props->type == TypeInt)
                          ass->value.setInt(ordinal);
                        else if (props->type == TypeBool)
                          ass->value.setBool(ordinal != 0);
                        else if (props->type == TypeString)
                          ass->value.setJString(svalue);
                        else if (props->type == TypeStructure)
                          ass->value.setJString(svalue);
                        else if (props->type == TypeEnum)
                          ass->value.setEnum(svalue.toUTF8(), ordinal);
                        
                        rscope->add(ass);
                    }
                }
            }
        }
    }
}

/**
 * I'd like to allow the syntax "symbol remove" instead of
 * "remove symbol" since that looks more like the assignment of a special
 * keyword value.  This is howeveer ambiguous if the parameter uses "remove"
 * in it's enumeration list.  In those cases it will resolve to the enumeration symbol
 * and if they want to remove it they have to use "remove symbol" instead.
 */
int MclParser::parseParameterOrdinal(Symbol* s, ParameterProperties* props,
                                     juce::String svalue, bool& isRemove)
{
    int ordinal = 0;

    if (props->type == TypeEnum) {
        ordinal = props->getEnumOrdinal(svalue.toUTF8());
        if (ordinal < 0) {
            if (svalue == "remove") {
                // hacky, hacky
                isRemove = true;
            }
            else {
                addError(juce::String("Invalid enumeration symbol: ") + svalue);
            }
            ordinal = 0;
        }
    }
    else if (props->type == TypeInt) {
        ordinal = svalue.getIntValue();
    }
    else if (props->type == TypeBool) {
        if (svalue == "true" || svalue == "1")
          ordinal = 1;
        else if (svalue != "false")
          addError(juce::String("Invalid boolean literal: ") + svalue);
    }
    else {
        // should not be here
        addError(juce::String("Parameter cannot have an ordinal value: ") + s->getName());
    }
    
    return ordinal;
}

/**
 * Unusual but parameters can be the names of other things.
 * At the moment this is only for sessionOverlay and trackOverlay
 */
void MclParser::validateStructureReference(Symbol* s, ParameterProperties* props,
                                           juce::String svalue)
{
    juce::String cls = props->structureClass;
    if (cls.length() == 0) {
        // this is actually my error, should have been annotated in symbols.xml
        Trace(1, "Missing structure name on symbol %s, assuming Overlay", s->getName());
        cls = "Overlay";
    }

    // ugh, no good utilities to ceal with these, revisit once we start having more than one
    if (cls == "Overlay") {
        ParameterSets* overlays = provider->getParameterSets();
        ValueSet* overlay = overlays->find(svalue);
        if (overlay == nullptr)
          addError(juce::String("Inivalid overlay name: ") + svalue);
    }
    else {
        addError(juce::String("Unable to deal with structure class: ") + svalue);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Bindings
//
// This is all rather brute force, but generalizing it isn't really
// necessary until something besides Binding objects comes along,
// and the result would be larger and harder to understand.
//
// things to ponder:
//
// Export should have all columns, don't need to be smart about
// compressed columns with defaults.  This means everything needs to be
// doable with columns which make boolean options like release, toggle, etc.
// awkward since they require a true/false column and are rare.
// More concise to have a single "options" column that is a csv of option flags
//
//////////////////////////////////////////////////////////////////////

/**
 * todo: add case insensitivy on this.
 */
bool MclParser::isKeyword(juce::String token, const char* keyword)
{
    return (token == keyword);
}

/**
 * Syntax: bindings <name> [replace|merge] [overlay|noOverlay]
 *
 * Merge assumes there is a unique way to identify source/target bindings.
 * The trigger is usually enough, but not if you allow multiple bindings per trigger.
 * trigger+symol handles that but doesn't get scope.
 * trigger+symbol+scope is always unique
 *
 * But if the purpose is to change the symbol associated with a binding then
 * you would match using only the trigger.  Evaluator will sort this out.
 * What's harder is merging partial Bindings into an existing one.  If all you wanted
 * to do is add arguments to a binding you still need to fully specify it in MCL.
 */
void MclParser::parseBinding(juce::StringArray& tokens)
{
    if (tokens.size() < 2) {
        addError("Missing BindingSet name");
    }
    else {
        juce::String oname = tokens[1];
        bool replace = false;
        // what we need here is a tri-state value for on/off/unspecified
        bool overlay = false;
        bool noOverlay = false;

        for (int i = 2 ; i < tokens.size() ; i++) {
            juce::String token = tokens[i];
            if (isKeyword(token, "replace"))
              replace = true;
            else if (isKeyword(token, "merge"))
              replace = false;
            else if (isKeyword(token, "overlay"))
              overlay = true;
            else if (isKeyword(token, "noOverlay"))
              noOverlay = true;
            else
              addError(juce::String("Invalid binding keyword: ") + token);
        }
        
        if (!hasErrors()) {
            MclSection* obj = new MclSection();
            obj->type = MclSection::Binding;
            obj->name = oname;
            obj->replace = replace;
            // to make this general could have a Map<String,String> 
            obj->bindingOverlay = overlay;
            obj->bindingNoOverlay = noOverlay;
            script->add(obj);
            currentSection = obj;
        }
    }
}

void MclParser::parseBindingLine(juce::StringArray& tokens)
{
    if (tokens.size() > 0) {
        juce::String keyword = tokens[0];
        if (keyword == "default")
          parseBindingDefault(tokens);
        else if (keyword == "columns" || keyword == "format")
          parseBindingColumns(tokens);
        else
          parseBindingObject(tokens);
    }
}

void MclParser::parseBindingDefault(juce::StringArray& tokens)
{
    int remainder = tokens.size() - 1;
    if ((remainder % 2) != 0)
      addError("Uneven number of default name/value tokens");
    else {
        int index = 1;
        while (index < tokens.size() && !hasErrors()) {
            juce::String name = tokens[index];
            juce::String value = tokens[index+1];
            
            if (name == "type") {
                bindingTrigger = parseTrigger(value);
            }
            else if (name == "channel") {
                bindingChannel = parseChannel(value);
            }
            else if (name == "scope") {
                if (validateBindingScope(value))
                  bindingScope = value;
            }
            else {
                // error message is confusing if they spell somthing wrong
                // there needs to be a distinction between "allowed but no default"
                // and "unrecongizied property"
                addError(juce::String("Property ") + name + " may not have a default");
            }
            index += 2;
        }
    }
}

void MclParser::parseBindingColumns(juce::StringArray& tokens)
{
    int column = 1;
    for (int i = 1 ; i < tokens.size() ; i++) {
        juce::String name = tokens[i];
        if (name == "type")
          typeColumn = column;
        else if (name == "channel")
          channelColumn = column;
        else if (name == "value")
          valueColumn = column;
        else if (name == "symbol")
          symbolColumn = column;
        else if (name == "scope")
          scopeColumn = column;
        else
          addError(juce::String("Invalid column name: ") + name);
        column++;
        if (hasErrors()) break;
    }
}

int MclParser::parseChannel(juce::String s)
{
    int channel = ToInt(s.toUTF8());
    if (channel < 1 || channel > 16) {
        addError(juce::String("Channel out of range: ") + s);
        channel = 0;
    }
    return channel;
}

Trigger* MclParser::parseTrigger(juce::String s)
{
    Trigger* trigger = nullptr;
    if (s == "note")
      trigger = TriggerNote;
    else if (s == "control" || s == "cc")
      trigger = TriggerControl;
    else if (s == "program" || s == "pgm")
      trigger = TriggerProgram;
    else if (s == "host")
      trigger = TriggerHost;
    else if (s == "key")
      trigger = TriggerKey;
    else
      addError(juce::String("Invalid trigger type: ") + s);
    return trigger;
}

int MclParser::parseMidiValue(juce::String s)
{
    int value = ToInt(s.toUTF8());
    if (value < 1 || value > 127) {
        addError(juce::String("Value out of range: ") + s);
        value = 0;
    }
    return value;
}

void MclParser::parseBindingObject(juce::StringArray& tokens)
{
    Binding* b = new Binding();
    b->trigger = bindingTrigger;
    b->midiChannel = bindingChannel;

    int index = 0;
    while (index < tokens.size() && !hasErrors()) {
        juce::String token = tokens[index];
        int column = index + 1;
        
        if (column == typeColumn) {
            b->trigger = parseTrigger(token);
        }
        else if (column == channelColumn) {
            b->midiChannel = parseChannel(token);
        }
        else if (column == valueColumn) {
            b->triggerValue = parseMidiValue(token);
        }
        else if (column == symbolColumn) {
            if (validateSymbol(token))
              b->setSymbolName(token.toUTF8());
        }
        else if (column == scopeColumn) {
            if (validateBindingScope(token))
              b->setScope(token.toUTF8());
        }
        else {
            // run out of specified columns
            break;
        }
        index++;
    }
    
    // what remains come in pairs except for release
    while (index < tokens.size() && !hasErrors()) {
        juce::String token = tokens[index];
        if (token == "release") {
            b->release = true;
            index++;
        }
        else {
            int next = index + 1;
            if (next >= tokens.size()) {
                addError(juce::String("Missing value for: ") + token);
            }
            else {
                juce::String value = tokens[next];
                if (token == "arguments" || token == "args") {
                    b->setArguments(value.toUTF8());
                }
                else if (token == "type") {
                    b->trigger = parseTrigger(value);
                }
                else if (token == "channel") {
                    b->midiChannel = parseChannel(value);
                }
                else if (token == "value") {
                    b->triggerValue = parseMidiValue(value);
                }
                else if (token == "symbol") {
                    if (validateSymbol(value))
                      b->setSymbolName(value.toUTF8());
                }
                else if (token == "scope") {
                    if (validateBindingScope(value))
                      b->setScope(value.toUTF8());
                }
                else {
                    addError(juce::String("Token not allowed: ") + token);
                }
            }
            index += 2;
        }
    }

    if (!hasErrors()) {
        MclSection* section = getSection();
        section->bindings.add(b);
    }
    else {
        delete b;
    }
}

bool MclParser::validateSymbol(juce::String name)
{
    bool valid = false;
    Symbol* s = provider->getSymbols()->find(name);
    if (s != nullptr)
      valid = true;
    else
      addError(juce::String("Invalid symbol name: ") + name);
    return valid;
}

bool MclParser::validateBindingScope(juce::String name)
{
    (void)name;
    // if it is an integer, trust it as long as it is positive
    // if it is a name, could check agains the defined group names
    // but may want to allow bindings with unresolved groups to go in anyway
    // and add the group later
    return true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
