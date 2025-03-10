
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../util/Util.h"
#include "../model/Symbol.h"
#include "../model/ParameterProperties.h"
#include "../model/ParameterSets.h"
#include "../model/ValueSet.h"
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

        else if (keyword == MclSection::KeywordBinding)
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
        }
    }
}

int MclParser::parseScopeId(juce::String token)
{
    int trackNumber = 0;
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
                    int ordinal = 0;
                    if (!isRemove) {
                        if (props->type == TypeStructure)
                          validateStructureReference(s, props, svalue);
                    
                        else if (props->type != TypeString)
                          ordinal = parseParameterOrdinal(s, props, svalue, isRemove);
                    }
                    
                    if (!hasErrors()) {
                        MclScope* rscope = getScope();
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
        addError(juce::String("Unable to deal with structure class %s: ") + svalue);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Bindings
//
//////////////////////////////////////////////////////////////////////

void MclParser::parseBinding(juce::StringArray& tokens)
{
    (void)tokens;
    addError(juce::String("Bindging sections not supported"));
}

void MclParser::parseBindingLine(juce::StringArray& tokens)
{
    (void)tokens;
    addError(juce::String("Bindging lines not supported"));
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
