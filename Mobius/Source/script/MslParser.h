
#pragma once

#include <JuceHeader.h>

#include "Tokenizer.h"

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

};
