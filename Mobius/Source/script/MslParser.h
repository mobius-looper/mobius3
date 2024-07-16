
#pragma once

#include <JuceHeader.h>

#include "MslTokenizer.h"
#include "MslModel.h"

/**
 * Emerging object to capture details and location of one parser error.
 */
class MslParserError
{
  public:

    MslParserError() {}
    ~MslParserError() {}

    juce::String details;
    int line = 0;
    int column = 0;
};

/**
 * Soon to be complex object conveying all the things that went wrong
 * during the parsing of a file.  If the parse succeeds, warnings may be
 * in the result but a Script object will have been created and returned.
 */
class MslParserResult
{
  public:

    MslParserResult() {}
    ~MslParserResult() {}

    /**
     * The full path to the file that was parsed.
     * Not set by the parser but may be added by the receiver.
     */
    juce::String sourceFile;

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
    class MslScript* script = nullptr;

    // quick and dirty list of error messages
    juce::StringArray errors;

    // evoving error details
    juce::OwnedArray<MslParserError> details;

};

class MslParser
{
  public:

    MslParser() {}
    ~MslParser() {}

    // new and unfinished interface we're moving toward
    MslParserResult* parseNew(juce::String source);
    

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
