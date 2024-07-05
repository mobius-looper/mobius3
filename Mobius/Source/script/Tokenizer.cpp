
#include <JuceHeader.h>

#include "Tokenizer.h"

/**
 * Load the tokenizer with content.
 */
void Tokenizer::setContent(juce::String s)
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

bool Tokenizer::hasNext()
{
    return !iterator.isEOF();
}

Token Tokenizer::next()
{
    Token t = Token(Token::Type::End);
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
        if (t.type == Token::Type::String) {
            // this tokenizer leaves the surrounding quotes on the token
            token =  token.unquoted();
        }
        t.value = token;
        
    }
    return t;
}

int Tokenizer::getLines()
{
    return document.getNumLines();
}

int Tokenizer::getLine()
{
    juce::CodeDocument::Position psn = iterator.toPosition();
    return psn.getLineNumber();
}

int Tokenizer::getColumn()
{
    juce::CodeDocument::Position psn = iterator.toPosition();
    return psn.getIndexInLine();
}

/**
 * Convert the CPPTokeniser type to one of ours.
 */
Token::Type Tokenizer::convertType(int cpptype)
{
    Token::Type type = Token::Type::Error;
    switch (cpptype) {
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_error:
            type = Token::Type::Error;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_comment:
            type = Token::Type::Comment;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_keyword:
            type = Token::Type::Symbol;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_operator:
            type = Token::Type::Operator;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_identifier:
            type = Token::Type::Symbol;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_integer:
            type = Token::Type::Int;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_float:
            type = Token::Type::Float;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_string:
            type = Token::Type::String;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_bracket:
            type = Token::Type::Bracket;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_punctuation:
            type = Token::Type::Punctuation;
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_preprocessor:
            type = Token::Type::Processor;
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
juce::String Tokenizer::toString(int cpptype)
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

