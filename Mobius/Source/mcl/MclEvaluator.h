
#pragma once

#include <JuceHeader.h>

class MclEvaluator
{
  public:

    MclEvaluator(class Provider* p);
    ~MclEvaluator();
    
    void eval(class MclScript* script, class MclResult& result);

  private:
    
    class Provider* provider = nullptr;
    class MclResult* result = nullptr;
    
    void addError(juce::String err);
    bool hasErrors();
    void addResult(class MclSection* section);
    void evalSession(class MclSection* section);
    void evalOverlay(class MclSection* section);
    void evalAssignment(class MclAssignment* ass, class ValueSet* dest);

    void evalBinding(class MclSection* section);
};
    
