/**
 * A few MSL support utilities that need to be shared by Supervisor
 * and MobiusKernel depending on which side the script is running on.
 *
 * Most of this deals with epanding scope references from scripts.
 * 
 */

#pragma once

#include <JuceHeader.h>

class ScriptUtil
{
  public:

    ScriptUtil() {}
    ~ScriptUtil() {}

    void initialize(class MslContext* c);
    void configure(class Session* s, class GroupDefinitions* g);
    void refresh(class Session* s);
    void refresh(class GroupDefinitions* g);

    int getMaxScope();
    bool isScopeKeyword(const char* cname);
    bool expandScopeKeyword(const char* name, juce::Array<int>& numbers);

  private:

    class MslContext* context = nullptr;
    class Session* session = nullptr;
    class GroupDefinitions* groups = nullptr;
    int audioTracks = 0;
    int midiTracks = 0;
    
};
