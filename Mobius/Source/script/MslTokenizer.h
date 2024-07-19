/**
 * A simple source tokenizer built on top of the Juce CPlusPlusCodeTokenizer.
 * 
 * MSL isn't nearly as complex as C++ but it has similar tokens.  The main
 * consequence of this is that old scripts used ! for preprocessor directives
 * and now it's better to use #
 *
 * I'm hiding the use of the Juce tokenizer and adding a private MslToken
 * model that captures things in a way that requires less typing, and adds
 * some basic token alayisis.
 *
 */
#pragma once

#include <JuceHeader.h>

class MslToken
{
  public:

    enum Type {
        End,
        Error,
        Comment,
        Symbol,
        String,
        Int,
        Float,
        Bool,
        Bracket,
        Punctuation,
        Operator,
        Processor
    };
    
    MslToken() {}
    MslToken(Type t) {type = t;}
    ~MslToken() {}
    
    Type type;
    juce::String value;
    int line = 0;
    int column = 0;

    bool isSymbol() {return type == Type::Symbol;}

    // when type is Bool, this must have the same logic
    // that the tokenizer used to decide it was a bool
    // it does when tokenized
    bool getBool() {
        return (value == "true");
    }

    bool isOpen() {
        return ((value == "{") || (value == "(") || (value == "["));
    }

  private:

};    

class MslTokenizer
{
  public:
    
    MslTokenizer() {}
    MslTokenizer(juce::String s) {setContent(s);}

    void setContent(juce::String s);
    bool hasNext();
    MslToken next();

    int getLines();
    int getLine();
    int getColumn();
    
  private:
    juce::CodeDocument document;
    juce::CodeDocument::Iterator iterator;

    juce::String content;
    juce::CPlusPlusCodeTokeniser tokeniser;
    
    MslToken::Type convertType(int cpptype);
    juce::String toString(int cpptype);

    
};
