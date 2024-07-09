
#pragma once

#include <JuceHeader.h>

#include "MslTokenizer.h"
#include "MslModel.h"

class MslParser
{
  public:

    MslParser() {}
    ~MslParser() {}

    class MslScript* parseFile(juce::String path, juce::String source);

    void prepare(MslScript* src);
    void consume(juce::String content);
    
    juce::StringArray* getErrors() {
        return &errors;
    }
    
  private:

    MslTokenizer tokenizer;

    // script being parsed
    MslScript* script = nullptr;

    // the parse stack
    MslNode* current = nullptr;

    juce::StringArray errors;

    void parse(juce::String source);
    void sift();
    
    void errorSyntax(MslToken& t, juce::String details);
    bool matchBracket(MslToken& t, MslNode* block);

    MslNode* checkKeywords(juce::String token);
    MslNode* push(MslNode* node);

    bool operandable(MslNode* node);
    int precedence(juce::String op1, juce::String op2);
    void unarize(MslToken& t, MslOperator* possible);
    MslNode* subsume(MslNode* op, MslNode* operand);

    
};
