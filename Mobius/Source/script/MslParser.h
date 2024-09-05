/**
 * A parser for the MSL language.
 * 
 * The parser consumes a string of text, and produces an MslCompilation object.
 * The parser lives at the shell level and does not need to use object pools.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "MslTokenizer.h"
#include "MslModel.h"
#include "MslCompilation.h"

class MslParser
{
  public:

    MslParser() {}
    ~MslParser() {}

    // usual file and scriptlet parsing interface
    MslCompilation* parse(juce::String source);

    // make this public so the MslModel classes can add token errors
    void errorSyntax(MslToken& t, juce::String details);
    
  private:

    MslTokenizer tokenizer;

    // the results being accumulated
    class MslCompilation* script = nullptr;
    
    // the root block of the parse
    class MslBlock* root = nullptr;

    // the parse stack
    class MslNode* current = nullptr;

    void init();
    void sift();
    void functionize(class MslFunctionNode* node);
    void functionize(class MslInitNode* node);
    void embody();
    
    void parseInner(juce::String source);
    void errorSyntax(MslNode* node, juce::String details);
    bool matchBracket(MslToken& t, class MslNode* block);

    MslNode* checkKeywords(MslToken& t);
    MslNode* push(MslNode* node);

    bool operandable(MslNode* node);
    int precedence(juce::String op1, juce::String op2);
    void unarize(MslToken& t, class MslOperator* possible);
    MslNode* subsume(MslNode* op, MslNode* operand);

    void parseDirective(MslToken& t);
    void parseArguments(MslToken& t, int offset, juce::String remainder);

};
