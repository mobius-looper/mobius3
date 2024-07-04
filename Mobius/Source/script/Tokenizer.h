
#pragma once

#include <JuceHeader.h>

class Token
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
    
    Token() {}
    Token(Type t) {type = t;}
    ~Token() {}
    
    Type type;
    juce::String value;

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

class Tokenizer
{
  public:
    
    Tokenizer() {}
    Tokenizer(juce::String s) {setContent(s);}

    void setContent(juce::String s);
    bool hasNext();
    Token next();

    int getLines();
    int getLine();
    int getColumn();
    
  private:
    juce::CodeDocument document;
    juce::CodeDocument::Iterator iterator;

    juce::String content;
    juce::CPlusPlusCodeTokeniser tokeniser;
    
    Token::Type convertType(int cpptype);
    juce::String toString(int cpptype);

    
};
