
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/MobiusConfig.h"
#include "../model/Session.h"
#include "../sync/Pulsator.h"

#include "ScriptUtil.h"

void ScriptUtil::initialize(Pulsator* p)
{
    pulsator = p;
}

void ScriptUtil::configure(MobiusConfig* c, Session* s)
{
    configuration = c;
    session = s;
}

int ScriptUtil::getMaxScope()
{
    return session->audioTracks + session->midiTracks;
}

bool ScriptUtil::expandScopeKeyword(const char* cname, juce::Array<int>& numbers)
{
    bool valid = true;
    juce::String name(cname);
    
    if (name.equalsIgnoreCase("all")) {
        int total = session->audioTracks + session->midiTracks;
        for (int i = 0 ; i < total ; i++) {
            numbers.add(i+1);
        }
    }
    else if (name.equalsIgnoreCase("audio")) {
        int total = session->audioTracks;
        for (int i = 0 ; i < total ; i++) {
            numbers.add(i+1);
        }
    }
    else if (name.equalsIgnoreCase("midi")) {
        int base = session->audioTracks + 1;
        for (int i = 0 ; i < session->midiTracks ; i++) {
            numbers.add(base + i);
        }
    }
    else if (name.equalsIgnoreCase("outSyncMaster")) {
        numbers.add(pulsator->getOutSyncMaster());
    }
    else if (name.equalsIgnoreCase("trackSyncMaster")) {
        numbers.add(pulsator->getTrackSyncMaster());
    }
    else if (name.equalsIgnoreCase("focused")) {
        // this depends on who manages focus, if it's a UI level thing
        // then Supervisor else Kernel
    }
    else if (name.equalsIgnoreCase("muted")) {
        // this requires access to kernel track state
        // generally it is safe to cross threads for this except
        // when track counts are being changed
        // this is a pretty severe change and we can prevent scripts
        // from running while that happens
        valid = false;
    }
    else if (name.equalsIgnoreCase("playing")) {
        // MOS has this as the opposite of muted
        valid = false;
    }
    else {
        // MOS has "group" which we don't need if we just
        // assume that anything other than a keyword can
        // be a group name
        int ordinal = configuration->getGroupOrdinal(name);
        if (ordinal < 0) {
            // didn't match, invalid keyword
            valid = false;
        }
        else {
            // groups are defined in the Setup which we have access to here
            // HOWEVER old MOS scripts can also change the group assignment
            // on the fly which would requires access to kernel track state
            valid = false;
        }
    }
    return valid;
}

