
#pragma once

#include <JuceHeader.h>

class Token
{
  public:

    enum Type {
        End,
        Error,
        Symbol,
        String,
        Int,
        Float,
        Bracket,
        Punctuation,
        Operator,
        Comment,
        Processor
    };
    
    Token() {}
    Token(Type t) {type = t;}
    ~Token() {}
    
    Type type;
    juce::String value;

    bool isSymbol() {return type == Type::Symbol;}

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

  private:
    juce::CodeDocument document;
    juce::CodeDocument::Iterator iterator;

    juce::String content;
    juce::CPlusPlusCodeTokeniser tokeniser;
    
    Token::Type convertType(int cpptype);
    juce::String toString(int cpptype);

    
};
