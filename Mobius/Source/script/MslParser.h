/**
 * A parser for the MSL language.
 * 
 * The parser consumes a string of text, and produces either
 * an MslScript containing the parse tree, or a list of errors to be displayed.
 *
 * There is an experimental interface for incremental line-at-a-time parsing
 * for the MobiusConsole.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "MslTokenizer.h"
#include "MslModel.h"
#include "MslScript.h"
#include "MslError.h"

/**
 * The parser may return two things: a finished MslScript object,
 * a list of errors encountered during parsing, or both.
 *
 * If a script is returned, it is suitable for installation and evaluation.
 * If an error list is returned, it should be retained and displayed to the user.
 *
 * If a fatal error is encountered, it will be in the error list and a script
 * is not returned.
 *
 * If only warnings are encountered, both a script and a warning list is returned.
 *
 * Currently there are no warnings.  You're going to jail son.
 *
 * The MslParserResult and it's contents are dynamically allocated and must
 * be disposed of by the receiver. 
 *
 */
class MslParserResult
{
  public:

    MslParserResult() {}
    ~MslParserResult() {}

    /**
     * The script that was succesfully parsed.
     */
    std::unique_ptr<class MslScript> script = nullptr;

    /**
     * Errors encountered during parsing.
     */
    juce::OwnedArray<class MslError> errors;

};

class MslParser
{
  public:

    MslParser() {}
    ~MslParser() {}

    // usual file parsing interface
    MslParserResult* parse(juce::String source);

    // parsing interface for scriptlets
    MslParserResult* parse(class MslScript* scriptlet, juce::String source);
    
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

    MslNode* checkKeywords(MslToken& t);
    MslNode* push(MslNode* node);

    bool operandable(MslNode* node);
    int precedence(juce::String op1, juce::String op2);
    void unarize(MslToken& t, MslOperator* possible);
    MslNode* subsume(MslNode* op, MslNode* operand);

    void parseDirective(MslToken& t);
    
};
