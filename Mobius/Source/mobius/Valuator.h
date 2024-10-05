/**
 * This is another in a string of attempts to bring some order to the chaos that
 * is parameter value bindings.   There are many parameters that effect the runtime
 * behavior of the audio and midi engines, values for these may be defined in files
 * such as session.xml and mobius.xml, they can be changed temporarily with .mos and .msl
 * scripts, and each track needs to maintain an independent value binding for most of them.
 *
 * Parameter values need to be accessible from both the UI and audio threads and bulk
 * collections of parameter values, ValueSets, need to be activated at will.
 *
 * The mechanics behind how all of this works is still evolving, but Valuator encapsulates
 * how that is done.  The engine simply asks Valuator for the value of a parameter, and
 * the value is determined by looking in a number of possible sources.
 *
 * Code in the engine may choose to listen for changes made by the UI or scripts and
 * respond accordingly.
 *
 * Since parameter bindings may be kept in HashMaps or other search structures, care must
 * be taken to prevent the audio thread from allocating memory.
 *
 * There will be one of these managed by MobiusShell and provided to code in the rest
 * of the kernel.  I'm not seeing a need right now to promote this upward to Supervisor,
 * the UI can can continue using UIAction and Query to access kernel parameter values, though
 * once the design settles down I may wish to revisit that.
 */

#pragma once

#include "../model/ParameterConstants.h"
#include "../model/Session.h"

class Valuator
{
  public:
    
    Valuator(class MobiusShell* s);
    ~Valuator();

    void initialize(class MobiusConfig* c, class Session* s);
    void reconfigure(class MobiusConfig* c, class Session* s);

    SyncSource getSyncSource(Session::Track* trackdef, SyncSource dflt);
    SyncTrackUnit getTrackSyncUnit(Session::Track* trackdef, SyncTrackUnit dflt);
    SyncUnit getSlaveSyncUnit(Session::Track* trackdef, SyncUnit dflt);

    int getParameterOrdinal(int trackId, SymbolId id);
    ParameterMuteMode getMuteMode(int trackId);
    SwitchLocation getSwitchLocation(int trackId);
    SwitchDuration getSwitchDuration(int trackId);
    SwitchQuantize getSwitchQuantize(int trackId);
    QuantizeMode getQuantizeMode(int trackId);
    EmptyLoopAction getEmptyLoopAction(int trackId);

    void bindParameter(int trackId, class UIAction* a);
    void clearBindings(int trackId);

  private:

    class Track {
      public:
        Track() {}
        ~Track() {}

        int number = 0;
        class MslBinding* bindings = nullptr;
        // temporary
        int activePreset = 0;
    };
    
    class MobiusShell* shell = nullptr;
    class MslEnvironment* msl = nullptr;
    class SymbolTable* symbols = nullptr;
    
    juce::OwnedArray<Track> audioTracks;
    int audioActive = 0;
    juce::OwnedArray<Track> midiTracks;
    int midiActive = 0;
    
    void initTracks();
    void clearBindings(Track* track);
    Track* getTrack(int number);
    class Preset* getPreset(Track* t);

};
