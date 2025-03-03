
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/old/MobiusConfig.h"
#include "../model/Session.h"

#include "MslContext.h"
#include "ScriptExternals.h"

#include "ScriptUtil.h"

void ScriptUtil::initialize(MslContext* c)
{
    context = c;
}

void ScriptUtil::configure(MobiusConfig* c, Session* s)
{
    configuration = c;
    session = s;
}

int ScriptUtil::getMaxScope()
{
    return session->getTrackCount();
}

/**
 * Group names are a bit of a problem here.
 * If the groups are defined at the time the scripts are loaded then
 * it will resolve, but if you add a new group, it won't be automatically
 * re-resolve old scripts that referenced it.  You'll have to touch
 * or reload the script.  Or just quote the string which makes it not
 * a symbol and doesn't need to be resolved.
 */
bool ScriptUtil::isScopeKeyword(const char* cname)
{
    bool keyword = false;
    juce::String name(cname);
    
    if (name.equalsIgnoreCase("all") ||
        name.equalsIgnoreCase("audio") ||
        name.equalsIgnoreCase("midi") ||
        name.equalsIgnoreCase("outSyncMaster") ||
        name.equalsIgnoreCase("trackSyncMaster") ||
        name.equalsIgnoreCase("focused") ||
        name.equalsIgnoreCase("muted") ||
        name.equalsIgnoreCase("playing")) {
        keyword = true;
    }
    else {
        int ordinal = configuration->getGroupOrdinal(name);
        keyword = (ordinal >= 0);
    }
    return keyword;
}

bool ScriptUtil::expandScopeKeyword(const char* cname, juce::Array<int>& numbers)
{
    bool valid = true;
    juce::String name(cname);
    
    if (name.equalsIgnoreCase("all")) {
        int total = session->getTrackCount();
        for (int i = 0 ; i < total ; i++) {
            numbers.add(i+1);
        }
    }
    else if (name.equalsIgnoreCase("audio")) {
        int total = session->getAudioTracks();
        for (int i = 0 ; i < total ; i++) {
            numbers.add(i+1);
        }
    }
    else if (name.equalsIgnoreCase("midi")) {
        int base = session->getAudioTracks() + 1;
        for (int i = 0 ; i < session->getMidiTracks() ; i++) {
            numbers.add(base + i);
        }
    }
    else if (name.equalsIgnoreCase("outSyncMaster")) {
        // this is the whole reason VarQuery exists, we need access to
        // variable implementations on both sides of the aisle
        // and it's easier to deal with than MslQuery which requires an MslExternal
        // which we don't have ready access to here
        // reconsider this, ScriptExternalId is basically just another symbol table
        // perhaps there should be a non-MSL way to access track variables and have
        // MslQuery forward through that
        // maybe it's better if this did context transitions in order to sxpand scopeps
        // must be a better way 
        VarQuery q;
        q.id = VarTransportMaster;
        if (context->mslQuery(&q)) {
            int tnum = q.result.getInt();
            if (tnum > 0)
              numbers.add(tnum);
        }
    }
    else if (name.equalsIgnoreCase("trackSyncMaster")) {
        VarQuery q;
        q.id = VarTrackSyncMaster;
        if (context->mslQuery(&q)) {
            int tnum = q.result.getInt();
            if (tnum > 0)
              numbers.add(tnum);
        }
    }
    else if (name.equalsIgnoreCase("focused")) {
        // this depends on who manages focus, if it's a UI level thing
        // then Supervisor else Kernel
        valid = false;
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

