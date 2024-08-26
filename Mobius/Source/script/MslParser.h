/**
 * A parser for the MSL language.
 * 
 * The parser consumes a string of text, and produces an MslScript object.
 * The parser lives at the shell level and does not need to use object pools.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "MslTokenizer.h"
#include "MslModel.h"
#include "MslScript.h"
#include "MslError.h"

class MslParser
{
  public:

    MslParser() {}
    ~MslParser() {}

    // usual file and scriptlet parsing interface
    MslScript* parse(juce::String source);

    // incremental interface for the console
    // this reuses an existing MslScript so function and variable definitions
    // can be accumulated over multiple interactive console lines
    bool parse(class MslScript* script, juce::String source);

    // make this public so the MslModel classes can add token errors
    void errorSyntax(MslToken& t, juce::String details);
    
  private:

    MslTokenizer tokenizer;

    // script being parsed
    MslScript* script = nullptr;

    // the parse stack
    MslNode* current = nullptr;

    void resetResult();
    void parseInner(juce::String source);
    void sift();
    void addFunction(class MslFunction* func);
    void addVariable(class MslVariable* var);
    
    void errorSyntax(MslNode* node, juce::String details);
    bool matchBracket(MslToken& t, class MslNode* block);

    MslNode* checkKeywords(MslToken& t);
    MslNode* push(MslNode* node);

    bool operandable(MslNode* node);
    int precedence(juce::String op1, juce::String op2);
    void unarize(MslToken& t, class MslOperator* possible);
    MslNode* subsume(MslNode* op, MslNode* operand);

    void parseDirective(MslToken& t);
    
};
