
#pragma once

#include <JuceHeader.h>

#include "MslTokenizer.h"
#include "MslModel.h"
#include "MslScript.h"

class MslParserResult
{
  public:

    MslParserResult() {}
    ~MslParserResult() {delete script;}

    /**
     * The full path to the file that was parsed.
     * Not set by the parser but may be added by the receiver.
     */
    juce::String path;

    /**
     * The source code that was parsed.
     * Retained so that a tool that displays errors can show more of
     * the surrounding context.
     */
    juce::String source;
    
    /**
     * The script that was succesfully parsed.
     * This is dynamically allocated and management must be taken over
     * by the receiver of the result.
     */
    std::unique_ptr<class MslScript> script = nullptr;

    // quick and dirty list of error messages
    //juce::StringArray errors;

    // evoving error details
    juce::OwnedArray<class MslError> errors;

};

class MslParser
{
  public:

    MslParser() {}
    ~MslParser() {}

    // usual file parsing interface
    MslParserResult* parse(juce::String source);

    // experimental interactive interface
    // these are broken, think more about it or get rid of them
    void prepare(MslScript* src);
    MslParserResult* consume(juce::String content);
    
  private:

    MslTokenizer tokenizer;

    // resuilt being generated
    MslParserResult* result = nullptr;
    
    // script being parsed
    MslScript* script = nullptr;

    // the parse stack
    MslNode* current = nullptr;

    void resetResult();
    void parseInner(juce::String source);
    void sift();
    
    void errorSyntax(MslToken& t, juce::String details);
    void errorSyntax(MslNode* node, juce::String details);
    bool matchBracket(MslToken& t, MslNode* block);

    MslNode* checkKeywords(juce::String token);
    MslNode* push(MslNode* node);

    bool operandable(MslNode* node);
    int precedence(juce::String op1, juce::String op2);
    void unarize(MslToken& t, MslOperator* possible);
    MslNode* subsume(MslNode* op, MslNode* operand);

    
};
