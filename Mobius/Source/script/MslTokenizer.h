
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
