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
     * This is only called once.
     */
    void initialize(class SymbolTable* symbols, bool isPlugin);

    /**
     * The bulk of what the vault does comes from the Session,
     * and the ParameterSets.  GroupDefinitions re provided for validation
     * or trackGroup ordinal range.
     *
     * A full refresh is done whenever any of these change.
     */
    void refresh(class Session* s, class Session::Track* t,
                 class ParameterSets* sets, class GroupDefinitions* groups);

    void refresh(class Session* s, class Session::Track* t);
    void refresh(class ParameterSets* sets);
    void refresh(class GroupDefinitions* groups);

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
     * A few functions can indirectly set parameters.
     * When those are intercepted by LogicalTrack it needs to call this to make
     * the corresponding change in the vault.
     */
    void setOrdinal(class Symbol* s, int ordinal);
    void setOrdinal(SymbolId id, int ordinal);
    
    /**
     * Basically the same as getOrdinal depositing the result in the Query.
     */
    bool doQuery(class Query* q);

    /**
     * Process a parameter action.
     * This will also detect and handle changes to overlays.
     */
    void doAction(class UIAction* a);

    // try to get rid of this, used by LogicalTrack::refreshState
    int getTrackOverlayNumber();

    // used in the implementation of LogicalTrack::unbindFeedback
    void unbind(SymbolId id);
    
  private:

    class SymbolTable* symbols = nullptr;
    bool isPlugin = false;
    class Session* session = nullptr;
    class Session::Track* track = nullptr;
    class ParameterSets* parameterSets = nullptr;
    class GroupDefinitions* groupDefinitions = nullptr;
    class ValueSet* sessionOverlay = nullptr;
    class ValueSet* trackOverlay = nullptr;

    juce::Array<int> sessionOrdinals;
    juce::Array<int> localOrdinals;

    void initArray(juce::Array<int>& array, int size);
    void refresh();
    void install(juce::Array<int>& ordinals);

    // Overlay shit to do flattening
    class ValueSet* findSessionOverlay(class ValueSet* globals);
    class ValueSet* findTrackOverlay(class ValueSet* globals, class ValueSet* track);
    class ValueSet* findOverlay(const char* name);
    class ValueSet* findOverlay(int ordinal);
    int getParameterIndex(SymbolId id);
    int getParameterIndex(class Symbol* s);
    int getLocalOrdinal(SymbolId id);
    void verifyOverlay(SymbolId overlayId);
    void fixOverlayOrdinal(SymbolId id, int ordinal);

    // Query and Action
    bool isValidOrdinal(class Symbol* s, class ParameterProperties* props, int value);
    void doOverlay(class UIAction* a);
    void doOverlay(juce::String ovname);
    void setOverlay(SymbolId sid, class ValueSet* overlay);

    // Flattener
    // if this turns out not to e used outside the vault, doesn't need to be static
    // tracks can only be refreshed one at a time so we can reuse this one array
    static juce::Array<int> flattener;

    void flatten(class ValueSet* defaults, class ValueSet* trackValues,
                 juce::Array<int>& result);

    int resolveOrdinal(class Symbol* symbol, 
                       class ValueSet* defaults, class ValueSet* trackValues);

    int resolveOrdinal(class Symbol* s, class ParameterProperties* props, class MslValue* v);
    int resolveEnum(class Symbol* s, class ParameterProperties* props, class MslValue* v);
    int resolveStructure(class Symbol* s, class ParameterProperties* props, class MslValue* v);

    void promotePorts();
    void promotePortAction(class Symbol* s, class ParameterProperties* props, int value);
    int getSessionOrdinal(SymbolId sid);
    void setSessionOrdinal(SymbolId sid, int value);
    void setLocalOrdinal(SymbolId sid, int value);
    
};
