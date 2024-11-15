/**
 * A few MSL support utilities that need to be shared by Supervisor
 * and MobiusKernel depending on which side the script is running on.
 */

#pragma once

#include <JuceHeader.h>

class ScriptUtil
{
  public:

    ScriptUtil() {}
    ~ScriptUtil() {}

    void initialize(class Pulsator* p);
    void configure(class MobiusConfig* c, class Session* s);

    int getMaxScope();
    bool expandScopeKeyword(const char* name, juce::Array<int>& numbers);

  private:

    class Pulsator* pulsator = nullptr;
    class MobiusConfig* configuration = nullptr;
    class Session* session = nullptr;
    int audioTracks = 0;
    int midiTracks = 0;
    
};
