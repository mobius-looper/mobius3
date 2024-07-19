
#include <JuceHeader.h>

#include "MslTokenizer.h"

/**
 * Load the tokenizer with content.
 */
void MslTokenizer::setContent(juce::String s)
{
    // do we need this or can it just live in the document?
    // leading whitespace seems to be included with the first token so trim
    content = s.trim();
    document.replaceAllContent(content);
    // is this how you reset these?  seems weird it doesn't
    // have any setPosition interfaces
    iterator = juce::CodeDocument::Iterator(document);
    // any difference here?
    //iterator = juce::CodeDocument::Iterator(document, 0, 0);
}    

bool MslTokenizer::hasNext()
{
    return !iterator.isEOF();
}

MslToken MslTokenizer::next()
{
    MslToken t = MslToken(MslToken::Type::End);
    t.line = getLine();
    t.column = getColumn();
    
    if (!iterator.isEOF()) {
        // any need to keep one of these?  how expensive is the constructor?
        juce::CPlusPlusCodeTokeniser toker;
        juce::CodeDocument::Position start = iterator.toPosition();
        int type = toker.readNextToken(iterator);
        juce::CodeDocument::Position end = iterator.toPosition();

        juce::String token = document.getTextBetween(start, end);
        // this does not appear to trim leading whitespace
        token = token.trimStart();
        t.type = convertType(type);
        if (t.type == MslToken::Type::String) {
            // this tokenizer leaves the surrounding quotes on the token
            token =  token.unquoted();
        }
        t.value = token;
        // hmm, if you capture the position here it is after the token
        // which looks worse than if you do it before and get leading whitespace
        //t.line = getLine();
        //t.column = getColumn();
    }
    return t;
}

int MslTokenizer::getLines()
{
    return document.getNumLines();
}

int MslTokenizer::getLine()
{
    juce::CodeDocument::Position psn = iterator.toPosition();
    return psn.getLineNumber();
}

int MslTokenizer::getColumn()
{
    juce::CodeDocument::Position psn = iterator.toPosition();
    return psn.getIndexInLine();
}

/**
 * Convert the CPPTokeniser type to one of ours.
 */
MslToken::Type MslTokenizer::convertType(int cpptype)
{
    MslToken::Type type = MslToken::Type::Error;
    switch (cpptype) {
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_error:
            type = MslToken::Type::Error;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_comment:
            type = MslToken::Type::Comment;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_keyword:
            type = MslToken::Type::Symbol;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_operator:
            type = MslToken::Type::Operator;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_identifier:
            type = MslToken::Type::Symbol;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_integer:
            type = MslToken::Type::Int;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_float:
            type = MslToken::Type::Float;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_string:
            type = MslToken::Type::String;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_bracket:
            type = MslToken::Type::Bracket;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_punctuation:
            type = MslToken::Type::Punctuation;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_preprocessor:
            type = MslToken::Type::Processor;
            break;
        default:
            // not in the enum
            break;
    }
    return type;
}

/**
 * Converts the CPlusPlusCodeTokenizer type to a string for debugging.
 */
juce::String MslTokenizer::toString(int cpptype)
{
    juce::String str;
    switch (cpptype) {
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_error:
            str = "error";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_comment:
            str = "comment";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_keyword:
            str = "keyword";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_operator:
            str = "operator";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_identifier:
            str = "identifier";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_integer:
            str = "integer";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_float:
            str = "float";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_string:
            str = "string";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_bracket:
            str = "bracket";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_punctuation:
            str = "punctuation";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_preprocessor:
            str = "preprocessor";
            break;
        default:
            str = "unknown";
            break;
    }
    return str;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

