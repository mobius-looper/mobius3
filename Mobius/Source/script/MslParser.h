
#pragma once

#include <JuceHeader.h>

#include "MslTokenizer.h"
#include "MslModel.h"

class MslParser
{
  public:

    MslParser() {}
    ~MslParser() {}

    class MslNode* parse(juce::String source);
    juce::StringArray* getErrors();

  private:

    juce::StringArray errors;
    MslTokenizer tokenizer;
    
    void errorSyntax(MslToken& t, juce::String details);
    bool matchBracket(MslToken& t, MslNode* block);

    MslNode* checkKeywords(juce::String token);
    MslNode* push(MslNode* current, MslNode* node);

    bool operandable(MslNode* node);
    int precedence(juce::String op1, juce::String op2);
    void unarize(MslToken& t, MslNode* current, MslOperator* possible);
    MslNode* subsume(MslNode* op, MslNode* operand);

    
};
