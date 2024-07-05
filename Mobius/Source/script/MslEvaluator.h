
#pragma once

class MslEvaluator
{
  public:

    MslEvaluator() {}
    ~MslEvaluator() {}

    bool trace = false;
    
    juce::String start(class MslNode* node);
    juce::StringArray* getErrors();
    
    // do I need a bunch of friends or will just MslNode work
    // to make these protected?
    juce::String evalBlock(class MslBlock* block);
    juce::String evalSymbol(class MslSymbol* node);
    juce::String evalLiteral(class MslLiteral* lit);
    juce::String evalOperator(class MslOperator* op);

  private:

    juce::StringArray errors;
    
    juce::String eval(class Symbol* s);
    juce::String invoke(class Symbol* s);
    juce::String query(class Symbol* s);


};
