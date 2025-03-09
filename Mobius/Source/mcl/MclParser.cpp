
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
    juce::StringArray tokens = juce::StringArray::fromTokens(line, " :", "\"");

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
                addError("Too many tokens");
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
                addError("Too many tokens");
            }
            else {
                MclObjectScope* obj = new MclObjectScope();
                obj->type = MclObjectScope::Overlay;
                script->add(obj);
                currentObject = obj;
            }
        }
        else if (keyword == "scope") {
            if (tokens.size() != 2) {
                addError("Missing scope identifier");
            }
            else {
                MclRunningScope* s = new MclRunningScope();
                s->scopeId = tokens[1];
                MclObjectScope* oscope  = getObject();
                oscope->add(s);
            }
        }
        else {
            // must be an assignment
            if (tokens.size() == 2) {
                Symbol* s = provider->getSymbols()->find(keyword);
                if (s == nullptr) {
                    addError("Unknown symbol " + keyword);
                }
                else {
                    MclRunningScope* rscope = getScope();
                    MclAssignment* ass = new MclAssignment();
                    ass->name = keyword;
                    ass->symbol = s;
                    ass->svalue = tokens[1];
                    rscope->add(ass);
                }
            }
            else if (tokens.size() == 3) {
                // todo: scoped assignment
                addError("Syntax error");
            }
            else {
                addError("Syntax error");
            }
        }
    }
}

void MclParser::addError(juce::String err)
{
    result->errors.add(juce::String("Line ") + juce::String(lineNumber) + ": " + err);
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
