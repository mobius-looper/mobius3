/**
 * TODO:
 *   
 * The validation being done here is going to be duplicated for every track
 * when it gets to the session defaults layer.  Could optimize that out
 * but it adds complexity and isn't that much.
 *
 * All parameters are going to be processed, even globals that are not
 * technically accessible by the tracks and can't be overridden.  Could reduce
 * the size of the ordinal arrays to only those relevant for track bindings,
 * but then global bindings for other Kernel components have to be handled a
 * different way.
 *
 * The Kernel components need something almost exactly like this for
 * the globals like transportTempo, hostBeatsPerBar, etc.  The values come
 * from the session and need to be validated, and the user is allowed to make
 * temporary assignments that will be reverted on Reset.  They need to support
 * both Session loading and UIAction/Query.  Each of these is handlinhg this
 * in a different way.  The core of the vault could be factored out for
 * a GlobalParameterVault that doesn't have any of the track-specific stuff in it.
 *
 * MidiInputDevice and MidiOutputDevice are currently String parameters but they
 * could be Structures and managed with ordinals like other structures.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"
#include "../../model/Session.h"
#include "../../model/ValueSet.h"
#include "../../model/ParameterSets.h"
#include "../../model/GroupDefinition.h"
#include "../../model/Query.h"
#include "../../model/UIAction.h"

#include "../../script/MslValue.h"

#include "ParameterVault.h"

ParameterVault::ParameterVault()
{
}

ParameterVault::~ParameterVault()
{
}

void ParameterVault::initialize(SymbolTable* syms, bool plugin)
{
    symbols = syms;
    isPlugin = plugin;

    // go ahead and flesh these out now so we can reduce potential
    // memory allocations after the initialization phase
    juce::Array<Symbol*>& params = symbols->getParameters();
    initArray(sessionOrdinals, params.size());
    initArray(localOrdinals, params.size());

    // only need to do this once, but don't have a convenient place
    // to do that
    flattener.ensureStorageAllocated(params.size());
}

void ParameterVault::initArray(juce::Array<int>& array, int size)
{
    array.clearQuick();
    for (int i = 0 ; i < size ; i++)
      array.add(-1);
}

/**
 * Clear local bindings after a Reset/TrackReset/GlobalReset function.
 *
 * This is where the old "reset retains" parameter went into play.  That
 * mutated into the SymbolProperties and still exists in some form but I'm
 * redesigning how all that works so this just unconditionally clears
 * everything atm.
 */
void ParameterVault::resetLocal()
{
    for (int i = 0 ; i < localOrdinals.size() ; i++)
      localOrdinals.set(i, -1);
}

void ParameterVault::unbind(SymbolId id)
{
    int index = getParameterIndex(id);
    if (index >= 0)
      localOrdinals.set(index, -1);
}

/**
 * Rebuild the vault after a change to any one of these objects.
 * Reloading the Session requres passing both the Session and the Track.
 * When ParameterSets or GropuDefinitions change, the Session and Track
 * are often the same object we used the last time.  Or the caller can
 * use one of the more focused refresh() functions, but they all do the
 * same thing.
 */
void ParameterVault::refresh(Session* s, Session::Track* t, ParameterSets* sets,
                             GroupDefinitions* groups)
{
    // because we can call refresh several times during the messy initialization period
    // detect whether we already have exactly the same things as before and skip
    // refresh
    if (session != s || track != t || parameterSets != sets || groupDefinitions != groups) {
        session = s;
        track = t;
        parameterSets = sets;
        groupDefinitions = groups;
        refresh();
    }
}

void ParameterVault::refresh(Session* s, Session::Track* t)
{
    if (session != s || track != t) {
        session = s;
        track = t;
        refresh();
    }
}

void ParameterVault::refresh(ParameterSets* sets)
{
    if (parameterSets != sets) {
        parameterSets = sets;
        refresh();
    }
}

void ParameterVault::refresh(GroupDefinitions* groups)
{
    if (groupDefinitions != groups) {
        groupDefinitions = groups;
        // is there any need to refresh here?
        // the only thing this could do is detect when the session references
        // a trackGroup that is now out of range and put it down to zero
        refresh();
    }
}

void ParameterVault::refresh()
{
    ValueSet* defaults = session->ensureGlobals();
    sessionOverlay = findSessionOverlay(defaults);
    
    ValueSet* trackValues = nullptr;
    trackOverlay = nullptr;

    // this may be omitted when building a vault containing only
    // global parameters that can't have track overrides
    if (track != nullptr) {
        trackValues = track->ensureParameters();
        trackOverlay = findTrackOverlay(defaults, trackValues);
    }

    // flattening is going to encounter both sessionOverlay and trackOverlay
    // as parameters and do the same name validation we just did
    // with findOverlay, we could pre-emptively force those ordinals
    // into the arraysince we know them now, and then ignore them during flattening
    // or verify them after flattening
    
    flatten(defaults, trackValues, flattener);
    promotePorts();
    install(flattener);

    // verify that the overlays found during flattening are the same ones we
    // used to do the flattening
    // I'm really hating how much complexity overlays inject here
    verifyOverlay(ParamSessionOverlay);
    verifyOverlay(ParamTrackOverlay);
}

/**
 * Immediately after flattening, promote either audioXPort or pluginXPort
 * to just inputPort and outputPort which is what the system uses.
 * The session editor sets both pairs, but we only use one at runtime
 * and it's too inconvenient making everything understand the difference.
 *
 * This operates onlyh on the flattened layer, not the local bindings.
 * It is rare for there to be bindings, but if that happens those take priority
 * and the promotion would have happened in doAction.
 */
void ParameterVault::promotePorts()
{
    SymbolId inputSid;
    SymbolId outputSid;

    if (isPlugin) {
        inputSid = ParamPluginInputPort;
        outputSid = ParamPluginOutputPort;
    }
    else {
        inputSid = ParamAudioInputPort;
        outputSid = ParamAudioOutputPort;
    }

    // these are NOT in the session, and even if they got in accidentally
    // they are stale and will be replaced

    setSessionOrdinal(ParamInputPort, getSessionOrdinal(inputSid));
    setSessionOrdinal(ParamOutputPort, getSessionOrdinal(outputSid));
}

int ParameterVault::getSessionOrdinal(SymbolId sid)
{
    int ordinal = 0;
    int index = getParameterIndex(sid);
    if (index >= 0)
      ordinal = sessionOrdinals[index];
    return ordinal;
}

void ParameterVault::setSessionOrdinal(SymbolId sid, int value)
{
    int index = getParameterIndex(sid);
    if (index >= 0)
      sessionOrdinals.set(index, value);
}

void ParameterVault::setLocalOrdinal(SymbolId sid, int value)
{
    int index = getParameterIndex(sid);
    if (index >= 0)
      localOrdinals.set(index, value);
}

/**
 * Install a flattened ordinal array.
 */
void ParameterVault::install(juce::Array<int>& ordinals)
{
    // sanity checks, should never happen
    if (ordinals.size() != sessionOrdinals.size()) {
        Trace(1, "ParameterVault: Mismatched session ordinal arrays %d %d",
              ordinals.size(), sessionOrdinals.size());
    }
    else if (ordinals.size() != localOrdinals.size()) {
        Trace(1, "ParameterVault: Mismatched local ordinal arrays %d %d",
              ordinals.size(), localOrdinals.size());
    }
    else {
        for (int i = 0 ; i < ordinals.size() ; i++) {
            int current = sessionOrdinals[i];
            int neu = ordinals[i];
            if (current != neu) {
                // local binding goes away
                localOrdinals.set(i, -1);
                sessionOrdinals.set(i, neu);

                // temporary so I can watch what's happening
                // on initial load, this will always change from -1 to something
                // so suppress those
                if (current != -1)
                  Trace(2, "ParameterValue: Changing parameter %d from %d to %d",
                        i, current, neu);
            }
        }
    }
}

/**
 * This little dance happens a lot and it's getting annoying.
 * Think about making a search structure for this in the SymbolTable.
 */
int ParameterVault::getParameterIndex(SymbolId id)
{
    int index = -1;
    Symbol* s = symbols->getSymbol(id);
    if (s != nullptr && s->parameterProperties != nullptr) {
        index = s->parameterProperties->index;
    }
    return index;
}

int ParameterVault::getParameterIndex(Symbol* s)
{
    int index = -1;
    if (s != nullptr && s->parameterProperties != nullptr) {
        index = s->parameterProperties->index;
    }
    return index;
}

/**
 * After flattening, the two overlays will be encountered during the scan and their
 * ordinals left in the array.  The overlays selected to DO the flattening need
 * to have matching ordinals.  If they don't match it means something is
 * wrong either in overlay selection before flattening, or in the flattening algorithm.
 */
void ParameterVault::verifyOverlay(SymbolId overlayId)
{
    ValueSet* chosenOverlay = nullptr;
    if (overlayId == ParamSessionOverlay)
      chosenOverlay = sessionOverlay;
    else
      chosenOverlay = trackOverlay;

    int ordinal = getOrdinal(overlayId);
    
    if (chosenOverlay != nullptr) {
        if (ordinal != chosenOverlay->number) {
            Trace(1, "ParameterVault: Ordinal mismatch on overlay %s",
                  chosenOverlay->name.toUTF8());
            // adjust the ordinal in the array to match what we used to flatten
            fixOverlayOrdinal(overlayId, chosenOverlay->number);
        }
    }
    else if (ordinal > 0) {
        // this is more serious, we dodn't think we had an overlay but one was
        // found during flattening
        Trace(1, "ParameterVault: Overlay ordinal %d found flattening but was not used to flatten");
        // I guess fix this one too
        fixOverlayOrdinal(overlayId, 0);
    }
}

void ParameterVault::fixOverlayOrdinal(SymbolId id, int ordinal)
{
    int index = getParameterIndex(id);
    if (index >= 0) {
        // where we fix this is unclear, if there was a local binding
        // it should have been used but wasn't so the binding can be reset
        // and the proper ordinal stored in the session array
        localOrdinals.set(index, -1);
        sessionOrdinals.set(index, ordinal);
    }
}

ValueSet* ParameterVault::findSessionOverlay(ValueSet* globals)
{
    ValueSet* result = nullptr;
    
    // if we have a local binding for this, do we continue to use it
    // or revert back to the session?  I think we use it, loading a session
    // does not necessarily clear local bindings for everything so if this
    // was left behind by the Reset logic then it applies
    int ordinal = getLocalOrdinal(ParamSessionOverlay);
    if (ordinal >= 0) {
        result = findOverlay(ordinal);
    }
    else {
        // fall back to a name-based session search
        const char* ovname = globals->getString("sessionOverlay");
        result = findOverlay(ovname);
    }
    return result;
}

int ParameterVault::getLocalOrdinal(SymbolId id)
{
    int ordinal = -1;
    int index = getParameterIndex(id);
    if (index >= 0)
      ordinal = localOrdinals[index];
    return ordinal;
}

ValueSet* ParameterVault::findTrackOverlay(ValueSet* globals, ValueSet* trackValues)
{
    ValueSet* result = nullptr;
    
    // same issues with local bindings
    int ordinal = getLocalOrdinal(ParamTrackOverlay);
    if (ordinal >= 0) {
        result = findOverlay(ordinal);
    }
    else {
        const char* ovname = trackValues->getString("trackOverlay");
        if (ovname == nullptr)
          ovname = globals->getString("trackOverlay");
        result = findOverlay(ovname);
    }
    return result;
}

ValueSet* ParameterVault::findOverlay(const char* ovname)
{
    ValueSet* overlay = nullptr;
    if (ovname != nullptr) {
        if (parameterSets == nullptr)
          Trace(1, "ParameterVault: No ParameterSets defined");
        else {
            overlay = parameterSets->find(juce::String(ovname));
            if (overlay == nullptr)
              Trace(1, "ParameterVault: Invalid parameter overlay name %s", ovname);
        }
    }
    return overlay;
}

ValueSet* ParameterVault::findOverlay(int ordinal)
{
    ValueSet* overlay = nullptr;
    if (ordinal > 0) {
        if (parameterSets == nullptr)
          Trace(1, "ParameterVault: No ParameterSets defined");
        else {
            overlay = parameterSets->getByOrdinal(ordinal);
            if (overlay == nullptr)
              Trace(1, "ParameterVault: Invalid parameter overlay ordinal %d", ordinal);
        }
    }
    return overlay;
}

//////////////////////////////////////////////////////////////////////
//
// Query
//
//////////////////////////////////////////////////////////////////////

int ParameterVault::getOrdinal(SymbolId id)
{
    return getOrdinal(symbols->getSymbol(id));
}

/**
 * LogicalTack wants this for some reason.
 * It should be the same as just asking for the ParamTrackOverlay ordinal.
 * refresh() was supposed to have verified this.
 */
int ParameterVault::getTrackOverlayNumber()
{
    return (trackOverlay != nullptr) ? trackOverlay->number : 0;
}

int ParameterVault::getOrdinal(Symbol* s)
{
    int ordinal = 0;
    if (s != nullptr && s->parameterProperties != nullptr) {
        int index = s->parameterProperties->index;
        if (index >= 0) {
            ordinal = localOrdinals[index];
            if (ordinal < 0) {
                ordinal = sessionOrdinals[index];
                if (ordinal < 0) {
                    // what the hell is this?
                    // this might happen if the session was missing some things
                    // that were added after it was saved
                    Trace(1, "ParameterVault: No ordinal installed for %s", s->getName());
                    // caller isn't accustomed to dealing with -1
                    ordinal = 0;
                }
            }
        }
    }
    return ordinal;
}

bool ParameterVault::doQuery(Query* q)
{
    int ordinal = getOrdinal(q->symbol);

    // LogicalTrack used to have special handl\ing for these, was that necessary?
    // this extra verification shold not be necessary if refresh() did it's job
    // check this for awhile but take out eventually
    switch (q->symbol->id) {
        case ParamTrackOverlay: {
            int other = 0;
            if (trackOverlay != nullptr)
              other = trackOverlay->number;
            if (other != ordinal)
              Trace(1, "ParameterVault: Mismatched track overlay ordinal on Query");
        }
            break;
            
        case ParamSessionOverlay: {
            int other = 0;
            if (sessionOverlay != nullptr)
              other = sessionOverlay->number;
            if (other != ordinal)
              Trace(1, "ParameterVault: Mismatched session overlay ordinal on Query");
        }
            break;

        default: break;
    }

    q->value = ordinal;

    // todo: should be checking whether the symbol in the query was in fact
    // something that can be managed by a track, that is, not a global
    return true;
}

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

/**
 * Direct ordinal assignment is used by LogicalTrack when it handles
 * a Function action that sets a parameter as a side effect.
 * Examples are FuncFocusLock and FunkTrackGroup.
 *
 * LT has determined what the ordinal should be after analyzing the
 * function arguments, and now wants to apply it.
 *
 * This is not expected to fail, but it might so LT should query
 * back the parameter if it wants to cache it to make sure the
 * request went through.
 */
void ParameterVault::setOrdinal(SymbolId id, int ordinal)
{
    // make it look like a normal UIAction on a parameter
    Symbol* s = symbols->getSymbol(id);
    if (s != nullptr) {
        UIAction a;
        a.symbol = s;
        a.value = ordinal;
        doAction(&a);
    }
}

/**
 * Handle a UIAction on a parameter
 * LT will already have filtered out function actions and Activation
 * actions that weren't for an overlay.
 */
void ParameterVault::doAction(UIAction* a)
{
    Symbol* s = a->symbol;
    SymbolId sid = s->id;
    if (sid == ParamTrackOverlay || sid == ParamSessionOverlay) {
        // well shucks, these are complicated
        doOverlay(a);
    }
    else if (a->symbol->behavior == BehaviorActivation) {
        // LogicalTrack will have already checked this
        juce::String ovname = s->name.fromFirstOccurrenceOf(Symbol::ActivationPrefixOverlay, false, false);
        doOverlay(ovname);
    }
    else {
        int index = getParameterIndex(s);
        if (index >= 0) {
            ParameterProperties* props = s->parameterProperties.get();
            if (isValidOrdinal(s, props, a->value)) {
                
                localOrdinals.set(index, a->value);

                promotePortAction(s, props, a->value);
            }
            else {
                // if they sent down an ordinal that was out of range
                // it could either be ignored, or go back to the default value of zero
                // for actions I think ignore it
                // if a session has values out of range those shoudl self-heal
            }
        }
    }
}

/**
 * If we just handled an action on one of the internal port symbols,
 * replicate it to the generic symbol.  This normally would not be done but
 * I guess if they do, maybe a test script, it needs to be visible to the system.
 *
 * If this was an action on the generic symbol, it could also be replicated
 * to the internal symbols, that is less necessaary, and we could get that during
 * "capture" of the runtime bindings back into the session.
 */
void ParameterVault::promotePortAction(Symbol* s, ParameterProperties* props, int value)
{
    (void)props;
    switch (s->id) {
        case ParamAudioInputPort: {
            if (!isPlugin) {
                setLocalOrdinal(ParamInputPort, value);
            }
        }
            break;
        case ParamAudioOutputPort: {
            if (!isPlugin) {
                setLocalOrdinal(ParamOutputPort, value);
            }
        }
            break;
        case ParamPluginInputPort: {
            if (isPlugin) {
                setLocalOrdinal(ParamInputPort, value);
            }
        }
            break;
        case ParamPluginOutputPort: {
            if (isPlugin) {
                setLocalOrdinal(ParamOutputPort, value);
            }
        }
            break;
    }
}

/**
 * This is used both to validate UIActions and when loading things from the Session
 */
bool ParameterVault::isValidOrdinal(Symbol* s, ParameterProperties* props, int value)
{
    bool valid = false;

    switch (props->type) {
        
        case TypeInt: {
            // !! todo: the IO port numbers have dynamic highs
            // in theory those could be passed down, but we don't necessarily
            // want to ignore the values in case they see an error and go
            // reconfigure the audio interface
            // range checking on those would ideally be done in the SessionEditor
            if (value >= props->low) {
                if (props->high == 0 || value <= props->high)
                  valid = true;
            }
        }
            break;
            
        case TypeBool: {
            // does't really matter, we just do zero/non-zero
            // but since -1 is used for unbound, require it be positive
            valid = (value >= 0);
        }
            break;
            
        case TypeEnum: {
            valid = (value >= 0 && value < props->values.size());
        }
            break;
        case TypeString: {
            // these can't be represented with ordinals,
            // should have caught this before getting here
            Trace(1, "ParameterVault: Attempted to set a String with an ordinal");
        }
            break;
            
        case TypeStructure: {
            // there are only two types we need to deal with
            // in both cases ordinal zero means "none"
            // so if a list has three objects, the range is 0-3 inclusive
            SymbolId sid = s->id;
            if (sid == ParamSessionOverlay || sid == ParamTrackOverlay) {
                int count = (parameterSets != nullptr) ? parameterSets->getSets().size() : 0;
                valid = (value >= 0 && value <= count);
            }
            else if (sid == ParamTrackGroup) {
                int count = (groupDefinitions != nullptr) ? groupDefinitions->groups.size() : 0;
                valid = (value >= 0 && value <= count);
            }
            else if (sid == ParamMidiInput || sid == ParamMidiOutput) {
                // these are harder to verify, would need the list of currently configured
                // devices passed in
                // I don't think we're actually dealing with these as ordinals ATM
                Trace(1, "ParameterVault: Assigning a midi device with an ordinal");
                valid = true;
            }
        }
            break;
            
        case TypeFloat: {
            // there is only one of these and it's an x100 int
            // could be smarter here
            valid = (value >= 0);
        }
            break;
    }
    return valid;
}

/**
 * When you set an overlay with an action, the flattening
 * process needs to happen all over again.
 */
void ParameterVault::doOverlay(UIAction* a)
{
    ValueSet* overlay = nullptr;
    bool forceOff = false;
    
    if (parameterSets == nullptr) {
        // not normal, assume this means all of them have been deleted?
        Trace(1, "ParameterVault: Attempt to assign overlay without ParameterSets");
    }
    else if (strlen(a->arguments) > 0) {
        overlay = parameterSets->find(juce::String(a->arguments));
        if (overlay == nullptr)
          Trace(1, "ParameterVault: Invalid overlay name %s in UIAction", a->arguments);
    }
    else if (a->value > 0) {
        overlay = parameterSets->getByOrdinal(a->value);
        if (overlay == 0)
          Trace(1, "ParameterVault: Invalid overlay ordinal %d in UIAction", a->value);
    }
    else {
        // an action with no value specified means to take away the
        // current overlay
        forceOff = true;
    }

    if (overlay != nullptr || forceOff)
      setOverlay(a->symbol->id, overlay);
}

/**
 * Here from a UIAction that is a BehavorActivation
 * with an overlay name.
 * Since we don't have a way of specifying track vs. session
 * the assumption is that always means the track overlay.
 */
void ParameterVault::doOverlay(juce::String ovname)
{
    ValueSet* overlay = nullptr;
    if (parameterSets == nullptr) {
        // not normal, assume this means all of them have been deleted?
        Trace(1, "ParameterVault: Attempt to assign overlay without ParameterSets");
    }
    else {
        overlay = parameterSets->find(ovname);
        if (overlay == nullptr)
          Trace(1, "ParameterVault: Invalid overlay name %s in UIAction",
                ovname.toUTF8());
    }

    if (overlay != nullptr)
      setOverlay(ParamTrackOverlay, overlay);
}

/**
 * Here from both styles of UIAction that want to set an overlay.
 * Install the overlay in the local cache, and reflatten.
 *
 * For actions, the new ValueSet will be nullptr if the overlay
 * identifier in the action was invalid, e.g. misspelled name or
 * ordinal out of range.  In those cases an error is logged but
 * we don't chanage the existing overlay.
 *
 * If the forceOff flag is set, it means the action deliberately
 * wanted to remove the overlay.
 */
void ParameterVault::setOverlay(SymbolId sid, ValueSet* overlay)
{
    ValueSet* target = nullptr;
    if (sid == ParamSessionOverlay)
      target = sessionOverlay;
    else
      target = trackOverlay;
    
    if (target != overlay) {

        // change it in the local bindings so it will Query back
        // correctly
        int ordinal = 0;
        if (overlay != nullptr) {
            int index = parameterSets->getSets().indexOf(overlay);
            ordinal = index + 1;
        }
        int index = getParameterIndex(sid);
        if (index >= 0)
          localOrdinals.set(index, ordinal);

        // set the local cache
        if (sid == ParamSessionOverlay)
          sessionOverlay = overlay;
        else
          trackOverlay = overlay;

        // flatten
        flatten(session->ensureGlobals(), track->ensureParameters(), flattener);
        promotePorts();
        install(flattener);

        // not necessary, but verify like we do with refresh()
    }
}

//////////////////////////////////////////////////////////////////////
//
// Flattening
//
//////////////////////////////////////////////////////////////////////

/**
 * Definition for the static array
 */
juce::Array<int> ParameterVault::flattener;

void ParameterVault::flatten(ValueSet* defaults, ValueSet* trackValues,
                             juce::Array<int>& result)
{
    result.clearQuick();

    juce::Array<Symbol*> parameters = symbols->getParameters();

    // usual goofyness with sparse arrays, initialize all of them to -1 to
    // indiciate there is no value, but most of these should be replaced
    for (int i = 0 ; i < parameters.size() ; i++)
      result.add(-1);

    for (auto param : parameters) {
        ParameterProperties* props = param->parameterProperties.get();
        if (props == nullptr) {
            Trace(1, "ParameterVault: Symbol got into the parameter list without properties %s",
                  param->getName());
        }
        else {
            int index = props->index;
            if (index < 0 || index >= parameters.size()) {
                Trace(1, "ParameterVault: Symbol %s has an invalid index %d",
                      param->getName(), index);
            }
            else {
                int ord = resolveOrdinal(param, defaults, trackValues);
                result.set(index, ord);
            }
        }
    }
}

/**
 * The interesting part.
 */
int ParameterVault::resolveOrdinal(Symbol* symbol, ValueSet* defaults, ValueSet* trackValues)
{
    int ordinal = -1;

    ParameterProperties* props = symbol->parameterProperties.get();
    // should have been caught long before now
    if (props == nullptr) return ordinal;
    
    if (trackOverlay != nullptr) {
        MslValue* v = trackOverlay->get(symbol->name);
        ordinal = resolveOrdinal(symbol, props, v);
    }

    if (ordinal < 0) {
        MslValue* v = trackValues->get(symbol->name);
        ordinal = resolveOrdinal(symbol, props, v);
    }

    if (ordinal < 0 && sessionOverlay != nullptr) {
        MslValue* v = sessionOverlay->get(symbol->name);
        ordinal = resolveOrdinal(symbol, props, v);
    }

    if (ordinal < 0) {
        MslValue* v = defaults->get(symbol->name);
        ordinal = resolveOrdinal(symbol, props, v);
    }

    // for new or empty sessions, use the default from the definition
    if (ordinal < 0) {
        if (props->type == TypeInt ||
            props->type == TypeBool ||
            props->type == TypeEnum) {
        
            ordinal = props->defaultValue;

            // if there is a low defined and it is greater than defaultValue
            // use it, that usually means defaultValue was uninitialized and left zero
            if (props->low > ordinal)
              ordinal = props->low;
        }
        else {
            // it is common for some things to be missing from new empty sessions
            // or parameters that are rarely used like sessionOverlay
            // avoid complaining about unresolved parameters by defauluting
            // everything to zero
            ordinal = 0;
        }
    }

    return ordinal;
}

/**
 * This is basically what the old Enumerator did.
 * Don't trust whatever came down in the ValueSet, do range checking on it
 * before we put it in the array.
 *
 * Return -1 on error so we can go to the next level.
 * Very much need error accumulation here.
 *
 * This could be doing more aggressive type coersion, like allowing an
 * MslValue::Bool for a TypeInt parameter.  But I think that actually hides
 * problems, so be strict about what types we expect to see.
 */
int ParameterVault::resolveOrdinal(Symbol* s, ParameterProperties* props, MslValue* v)
{
    int ordinal = -1;
    bool invalidType = false;

    // not supposed to see isNull in the session maps, filter it if it happens
    if (v != nullptr && !v->isNull()) {

        switch (props->type) {
            
            case TypeInt: {
                // allow Int or String, but the others are a modeling error
                // we usually allow Keyword for Strings, but it would be
                // weird to type "inputPort = :1"
                if (v->type == MslValue::Int ||
                    v->type == MslValue::String || v->type == MslValue::Keyword) {
                    int value = v->getInt();
                    if (isValidOrdinal(s, props, value))
                      ordinal = value;
                    else
                      Trace(1, "ParameterVault: Session value for %s out of range %d",
                            s->getName(), value);
                }
                else {
                    invalidType = true;
                }
            }
                break;
                
            case TypeBool: {
                // could do more range checking on these, really should be just
                // 0, 1, "true" or "false"
                if (v->type == MslValue::Int) {
                    ordinal = (v->getInt() > 0);
                }
                else if (v->type == MslValue::Bool) {
                    ordinal = v->getBool();
                }
                else if (v->type == MslValue::String ||
                         // using keywords is common in this case "midiThru = :true"
                         v->type == MslValue::Keyword) {
                    ordinal = (strcmp(v->getString(), "true") == 0);
                }
                else {
                    invalidType = true;
                }
            }
                break;
                
            case TypeEnum: {
                ordinal = resolveEnum(s, props, v);
            }
                break;
                
            case TypeString: {
                // Strings can't have ordinals
                // These should have been caught at a higher level if this
                // came from a UIAction, if we're flattening a Session just ignore it
            }
                break;
                
            case TypeStructure: {
                ordinal = resolveStructure(s, props, v);
            }
                break;
                
            case TypeFloat: {
                // only one of these, and it's an x100 int
                // I suppose since we have MslValue::Float we could allow that and
                // do the x100 conversion but we don't do Floats yet
                // Like TypeInt, support String values and coerce them
                if (v->type == MslValue::Int ||
                    v->type == MslValue::String || v->type == MslValue::Keyword)
                  ordinal = v->getInt();
                else
                  invalidType = true;
            }
                break;
        }

        if (invalidType) {
            Trace(1, "ParameterVault: Parameter %s given value with invalid type %d",
                  s->getName(), (int)(v->type));
        }
    }
    return ordinal;
}

/**
 * Contort an MslValue into an enumerated parameter ordinal.
 *
 * The session editor normally saves Enums.
 * Hand edited XML often leaves out the type and it becomes just a String.
 * Actions from MSL may use Keyword, e.g. "syncMode = :Host"
 * Ints would happen if the parameter was bound to a MIDI CC controller.
 *
 * When an Enum comes in from the session, we could trust either the symbolic name or
 * the ordinal.  Leaning toward always using the name since that almost never changes,
 * but it is sometimes necessary to reorder them or insert new values, so older saved sessions
 * might have invalid numbers.  If we're going to do that everywhere then there really
 * isn't any need to have MslType::Enum, is there?
 */
int ParameterVault::resolveEnum(Symbol* s, ParameterProperties* props, MslValue* v)
{
    int ordinal = -1;

    switch (v->type) {
        
        case MslValue::Int: {
            int value = v->getInt();
            if (isValidOrdinal(s, props, value))
              ordinal = value;
            else
              Trace(1, "ParameterVault: Parameter %s ordinal out of range %d",
                    s->getName(), value);
        }
            break;

        case MslValue::String:
        case MslValue::Keyword: {
            // results in -1 if not included 
            ordinal = props->values.indexOf(juce::String(v->getString()));
            if (ordinal < 0)
              Trace(1, "ParameterVault: Parameter %s invalid enumeration %s",
                    s->getName(), v->getString());
        }
            break;
            
        case MslValue::Enum: {
            // prefer the name but cross-check the ordinal
            // not necessary, but I like to know when this happens
            ordinal = props->values.indexOf(juce::String(v->getString()));
            if (ordinal < 0) {
                // name didn't match, was the index right?
                if (isValidOrdinal(s, props, v->getInt())) {
                    // in theory, the name could have changed but the position
                    // is still the same and we could use the original
                    // this would be rare and I think too dangerous to assume
                    Trace(1, "ParameterVault: Parameter %s invalid enumeration %s with valid ordinal %d",
                          s->getName(), v->getString(), v->getInt());
                }
                else {
                    // both are wrong, this is most likely a hand edited misplaced value
                    Trace(1, "ParameterVault: Parameter %s invalid enumeration %s",
                          s->getName(), v->getString(), v->getInt());
                }
            }
            else {
                // name was fine, what about the ordinal
                if (!isValidOrdinal(s, props, v->getInt())) {
                    Trace(1, "ParameterVault: Parameter %s had matching name %s but invalid ordinal %d",
                          s->getName(), v->getString(), v->getInt());
                    // I don't think it's worth dying for this.
                    // We can try to fix it, but if this came from the Session, it will still
                    // exist on disk and we'll see it again when the Session is reloaded.
                    // If we don't fix it though, we're going to log this every time the session
                    // is flattened which is annoying.  
                    v->fixEnum(ordinal);
                }
                else {
                    // shiny, captain
                }
            }
        }
            break;
                    
        default: {
            // Float, Bool, List, Keyword, Symbol
            // don't need to be agressive on coercing these
            Trace(1, "ParameterVault: Parameter %s given bizarre value type");
        }
            break;
    }
    return ordinal;
}

/**
 * Contort a value into a structure ordinal.
 *
 * UIActions can only use numbers right now, but soon they'll be able to
 * contain a full MslValue.
 *
 * The Session always stores structure references as Strings.
 *
 * The two MIDI devices are technically structures but we don't have everything
 * in place to treat them as such.  This needs more work.  Currently they are not
 * set or queried with actions, they can only be named in the session and
 * are handled directly by MidiTrack without going through the vault.  They
 * will have type='string' in the symbol.
 */
int ParameterVault::resolveStructure(Symbol* s, ParameterProperties* props, MslValue* v)
{
    (void)props;
    int ordinal = -1;
    bool invalidType = false;
    SymbolId sid = s->id;
    
    // for all structures, a value of 0 means "no selection" so
    // handle that early before we start thinking too hard
    if (v->type == MslValue::Int && v->getInt() == 0) {
        ordinal = 0;
    }
    else if (sid == ParamTrackOverlay || sid == ParamSessionOverlay) {
        if (parameterSets == nullptr) {
            Trace(1, "ParameterVault: No ParameterSets available for resolving overlay ordinals");
        }
        else {
            // during session flattening these were both pulled out early,
            // validated, and the ordinal forced in with setOrdinal
            // so it doesn't really matter what we do here, but shold be consistent
            // and will be validated after flattening
            if (v->type == MslValue::Int) {
                int value = v->getInt();
                // remember, ordidnals are 1 bvased for structures
                if (value >= 0 && value <= parameterSets->getSets().size())
                  ordinal = value;
                else
                  Trace(1, "ParameterVault: Parameter %s overlay ordinal out of range %d",
                        s->getName(), value);
            }
            else if (v->type == MslValue::String || v->type == MslValue::Keyword) {
                ValueSet* set = parameterSets->find(juce::String(v->getString()));
                if (set == nullptr) {
                    Trace(1, "ParameterVault: Parameter %s invalid overlay name %s",
                          s->getName(), v->getString());
                }
                else {
                    int index = parameterSets->getSets().indexOf(set);
                    ordinal = index + 1;
                }
            }
            else {
                invalidType = true;
            }
        }
    }
    else if (sid == ParamTrackGroup) {
        if (groupDefinitions == nullptr) {
            Trace(1, "ParameterVault: No GroupDefinitions available for resolving group ordinals");
        }
        else if (v->type == MslValue::Int) {
            int value = v->getInt();
            // remember structure ordinals are 1 based
            if (value >= 0 && value <= groupDefinitions->groups.size())
              ordinal = value;
            else
              Trace(1, "ParameterVault: Parameter %s group ordinal out of range %d",
                    s->getName(), value);
        }
        else if (v->type == MslValue::String || v->type == MslValue::Keyword) {
            GroupDefinition* def = groupDefinitions->find(juce::String(v->getString()));
            if (def == nullptr)
              Trace(1, "ParameterVault: Parameter %s invalid group name %s",
                    s->getName(), v->getString());
            else {
                int index = groupDefinitions->groups.indexOf(def);
                ordinal = index + 1;
            }
        }
        else {
            invalidType = true;
        }
    }
    else if (sid == ParamMidiInput || sid == ParamMidiOutput) {
        // since these aren't really in the vault, return zero rather
        // than -1 to stop walking through the layers since it won't be
        // in any of them
        ordinal = 0;
    }
    else {
        // since we're iterating over all symbols, this is going to pick up
        // things like ParamActiveLayout which are level='UI'
        // that are not handled down here, just ignore them
        ordinal = 0;
    }
    
    return ordinal;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
