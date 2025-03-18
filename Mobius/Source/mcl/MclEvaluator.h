
#pragma once

#include <JuceHeader.h>

#include "../model/Session.h"

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
    void evalAssignment(class MclSection* section, class MclAssignment* ass, class ValueSet* dest);
    void evalScopedAssignment(class MclSection* section, class Session* session, class MclAssignment* ass);
    void evalTrackName(class MclSection* section, class Session* session, class Session::Track* track, class MclAssignment* ass);
    void evalTrackType(class MclSection* section, class Session* session, class Session::Track* track, class MclAssignment* ass);

    void evalBinding(class MclSection* section);
};
    
