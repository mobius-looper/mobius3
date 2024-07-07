
#pragma once

#include <JuceHeader.h>

#include "Tokenizer.h"
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
    Tokenizer tokenizer;
    
    void errorSyntax(Token& t, juce::String details);
    bool matchBracket(Token& t, MslNode* block);

    MslNode* checkKeywords(juce::String token);
    MslNode* push(MslNode* current, MslNode* node);

    bool operandable(MslNode* node);
    int precedence(juce::String op1, juce::String op2);
    void unarize(Token& t, MslNode* current, MslOperator* possible);
    MslNode* subsume(MslNode* op, MslNode* operand);

    
};
