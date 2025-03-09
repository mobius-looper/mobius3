
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/Symbol.h"
#include "../Provider.h"

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
    for (auto line : lines) {
        parseLine(line);
        if (result->hasErrors())
          break;
        lineNumber++;
    }

    if (result->hasErrors()) {
        script.reset();
    }
    return script.release();
}

void MclParser::parseLine(juce::String line)
{
    // it is extremely common to write foo=bar like MSL scripts
    // do rather than "foo bar" without the equals
    // make tokenizing easier by just converting them to spaces
    // actually I like this approach for : scope prefixes as well
    // since you're not actually parsing the :
    line = line.replaceCharacters("=:", "  ");
    juce::StringArray tokens = juce::StringArray::fromTokens(line, " ", "\"");

    bool traceit = false;
    if (traceit) {
        Trace(2, "MclParser line %s", line.toUTF8());
        for (auto t : tokens)
          Trace(2, "  %s", t.toUTF8());
    }

    if (tokens.size() > 0) {

        juce::String keyword = tokens[0];
        if (keyword == "session") {
            if (tokens.size() > 2) {
                addError(line, "Too many tokens");
            }
            else {
                MclObjectScope* obj = new MclObjectScope();
                script->add(obj);
                currentObject = obj;
            }
        }
        else if (keyword == "overlay") {
            // todo: need to support the "memory" and "temporary" qualifiers
            if (tokens.size() > 2) {
                addError(line, "Too many tokens");
            }
            else if (tokens.size() < 2) {
                addError(line, "Missing overlay name");
            }
            else {
                MclObjectScope* obj = new MclObjectScope();
                obj->type = MclObjectScope::Overlay;
                obj->name = tokens[1];
                script->add(obj);
                currentObject = obj;
            }
        }
        else if (keyword == "scope") {
            if (tokens.size() != 2) {
                addError(line, "Missing scope identifier");
            }
            else {
                MclRunningScope* s = new MclRunningScope();
                s->scopeId = tokens[1];
                MclObjectScope* oscope  = getObject();
                oscope->add(s);
            }
        }
        else if (keyword == "remove" || keyword == "unset") {
            if (tokens.size() == 1) {
                addError(line, "Missing parameter name");
            }
            else {
                // todo: might want to prevent some things from being removed?
                MclRunningScope* rscope = getScope();
                MclAssignment* ass = new MclAssignment();
                ass->name = tokens[1];
                ass->remove = true;
                rscope->add(ass);
            }
        }
        else {
            juce::String scopeId;
            juce::String sname;
            juce::String svalue;
            int trackNumber = 0;

            if (tokens.size() == 1) {
                addError(line, "Missing tokens");
            }
            else if (tokens.size() == 2) {
                sname = tokens[0];
                svalue = tokens[1];
            }
            else if (tokens.size() == 3) {
                scopeId = tokens[0];
                sname = tokens[1];
                svalue = tokens[2];
            }
            else {
                addError(line, "Too many tokens");
            }

            if (sname.length() > 0) {
                Symbol* s = provider->getSymbols()->find(sname);
                if (s == nullptr) {
                    addError(line, "Unknown symbol " + sname);
                }
                else {
                    if (scopeId.length() > 0) {
                        trackNumber = scopeId.getIntValue();
                        if (trackNumber == 0)
                          addError(line, "Invalid track number");
                    }
                    
                    if (!hasErrors()) {
                        MclRunningScope* rscope = getScope();
                        MclAssignment* ass = new MclAssignment();
                        ass->name = sname;
                        ass->symbol = s;
                        ass->svalue = svalue;
                        ass->scopeId = scopeId;
                        ass->scope = trackNumber;
                        rscope->add(ass);
                    }
                }
            }
        }
    }
}

void MclParser::addError(juce::String line, juce::String err)
{
    result->errors.add(juce::String("Line ") + juce::String(lineNumber) + ": " + line);
    result->errors.add(err);
}

bool MclParser::hasErrors()
{
    return result->errors.size() > 0;
}

MclObjectScope* MclParser::getObject()
{
    if (currentObject == nullptr) {
        MclObjectScope* obj = new MclObjectScope();
        script->add(obj);
        currentObject = obj;
    }
    return currentObject;
}
        
MclRunningScope* MclParser::getScope()
{
    if (currentScope == nullptr) {
        MclObjectScope* obj = getObject();
        MclRunningScope* scope = new MclRunningScope();
        obj->add(scope);
        currentScope = scope;
    }
    return currentScope;
}
            
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
