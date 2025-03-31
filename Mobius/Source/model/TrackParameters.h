/**
 * Kernel model for managing track parameter values.
 *  
 * One of these is maintained within each LogicalTrack, they contain a flattened
 * array of parameter ordinals from the Session, and one containing transient overrides
 * that were set with scripts or UIActions and are not stored permanently in the Session.
 *
 * The arrays are indexed using ParameterProperties::index which was calculated by Symbolizer
 * and SymbolTable when it was finished loading the stock parameters.  This may not include
 * other non-standard parameters that were added by the core and not included in symbols.xml
 * and won't include user parameters defined by scripts.
 *
 * Utilities are provided for flattening and managing the local bindings to avoid changing them
 * if those parameters did not change during a session edit.
 *
 */

#pragma once

class TrackParameters
{
  public:

    // install a new flat session array over the existing ones
    // remove local bindings if the values changed but retain them if they didn't
    void install(juce::Array<int>& newSession);
    
    

    // utility to flatten a Session into an ordinal array
    static void flatten(class SymbolTable* symbols, class ParameterSets* sets,
                        class Session* s, int track, juce::Array<int>& result);

  private:

    juce::Array<int> session;
    juce::Array<int> local;


    static int resolveOrdinal(class Symbol* symbol, class ValueSet* defaults, class ValueSet* trackValues,
                              class ValueSet* sessionOverlay, class ValueSet* trackOverlay);
    
    static class ValueSet* findSessionOverlay(class Provider* p, class ValueSet* globals);
    static class ValueSet* findTrackOverlay(class Provider* p, class ValueSet* globals, class ValueSet* track);
    static class ValueSet* findOverlay(class Provider* p, const char* name);
    
};
