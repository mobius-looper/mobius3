/**
 * Kernel model for managing track parameter values.
 *
 * The Vault does a number of things.  It consumes the Session and
 * organizes the parameters for each track in ordinal arrays for fast access.
 * 
 * In this process it also validates ordinal ranges so that it is safe to perform
 * static casts on enumerations.  Various consistency checks are done to make sure
 * the things left in the Session fit against the parameter definitions in
 * ParameterProperties.
 *
 * The Vault is THE source for parameter values within the LogicalTrack and the
 * wrapped track implementations.  Inner tracks may choose to cache values outside the value
 * but need to be prepared to reload those caches when directed any time the vault changes.
 *
 * The various parameter layers in the Session are flattened into a single
 * array of parameter ordinals which is what system internals use exclusively.
 *
 * The Vault handles UIAction and Query on the LogicalTrack.
 *
 * For a handful of parameters that have special validation requirements beyond what
 * can be defined in ParameterProperties, the Vault should handle those too.
 *
 * Static utilities are provided for flattening the session, but I don't think those
 * are necessary now the way the vault has evolved.
 *
 * The Vault does NOT do side effects other than watching the two overlay
 * parameters: ParamTrackOverlay and ParamSessionOverlay which can impact the
 * values of other parameters for this track.
 *
 */

#pragma once

#include "../../model/SymbolId.h"

class ParameterVault
{
  public:

    ParameterVault();
    ~ParameterVault();

    /**
     * The vault requires a few system services to do it's thing.
     * This may be called more than once, symbols is not expected to change
     * but ParameterSets may be changed when a new collection is sent down
     * after editing.
     *
     * The vault is allowed to retain a reference to the ParameterSets and
     * must be reinitialized if that changes.
     */
    void initialize(class SymbolTable* symbols, class ParameterSets* sets);

    /**
     * Refresh the parameter sets (overlays) after editing.
     */
    void refresh(class ParameterSets* sets);

    /**
     * Install the Session parameters for one track.
     * The vault is allowed to retain a reference to the Session
     * and it's contents.
     */
    void loadSession(class Session* session, int trackNumber);

    /**
     * Remove all local parameter bindings.
     */
    void resetLocal();
        
    /**
     * Return the ordinal value of a parameter.
     * If this is not a Queryable parameter this should return 0 and log an error.
     */
    int getOrdinal(SymbolId id);
    int getOrdinal(class Symbol* s);

    /**
     * Basically the same as getOrdinal depositing the result in the Query.
     */
    bool doQuery(class Query* q);

    /**
     * Process a parameter action.
     * This will also detect and handle changes to overlays.
     */
    void doAction(class UIAction* a);
    
  private:

    class SymbolTable* symbols = nullptr;
    class ParameterSets* parameterSets = nullptr;
    class Session* session = nullptr;
    class Session::Track* track = nullptr;

    juce::Array<int> sessionOrdinals;
    juce::Array<int> localOrdinals;

    void initArray(juce::Array<int>& array, int size);
    void install(juce::Array<int>& ordinals);

    // needed by both doAction and flatten so it has to be static
    static bool isValidOrdinal(class ParameterProperties* props, int value);

    // Flattener
    // if this turns out not to e used outside the vault, doesn't need to be static

    // tracks can only be refreshed one at a time so we can reuse this one array
    static juce::Array<int> flattener;

    static void flatten(class SymbolTable* symbols, class ParameterSets* sets,
                        class Session* s, int track, juce::Array<int>& result);

    static int resolveOrdinal(class Symbol* symbol, class ValueSet* defaults, class ValueSet* trackValues,
                              class ValueSet* sessionOverlay, class ValueSet* trackOverlay);

    static int resolveOrdinal(class Symbol* s, class ParameterProperties* props, class MslValue* v);
    
    static class ValueSet* findSessionOverlay(class ParameterSets*, class ValueSet* globals);
    static class ValueSet* findTrackOverlay(class ParameterSets*, class ValueSet* globals, class ValueSet* track);
    static class ValueSet* findOverlay(class ParameterSets*, const char* name);
    
};
