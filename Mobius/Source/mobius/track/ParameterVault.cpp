/**
 * The validation being done here is going to be duplicated for every track
 * when it gets to the session defaults layer.  Could optimize that out
 * but it adds complexity and isn't that much.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"
#include "../../model/Session.h"
#include "../../model/ValueSet.h"
#include "../../model/ParameterSets.h"
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

void ParameterVault::initialize(SymbolTable* syms, ParameterSets* sets)
{
    symbols = syms;
    parameterSets = sets;

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

void ParameterVault::refresh(ParameterSets* sets)
{
    // !! this needs to reflatten when overlays change
    parameterSets = sets;
}

void ParameterVault::resetLocal()
{
    for (int i = 0 ; i < localOrdinals.size() ; i++)
      localOrdinals.set(i, -1);
}

void ParameterVault::loadSession(Session* s, int trackNumber)
{
    flatten(symbols, parameterSets, s, trackNumber, flattener);
    install(flattener);
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
                Trace(2, "ParameterValue: Changing parameter %d from %d to %d",
                      i, current, neu);
            }
        }
    }
}

int ParameterVault::getOrdinal(SymbolId id)
{
    return getOrdinal(symbols->getSymbol(id));
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
                    Trace(1, "ParameterVault: No ordinal installed for %s", s->getName());
                }
            }
        }
    }
    return ordinal;
}

bool ParameterVault::doQuery(Query* q)
{
    int ordinal = getOrdinal(q->symbol);

    // any mutations to do?
    
    q->value = ordinal;

    // todo: should be checking whether the symbol in the query was in fact
    // something that can be managed by a track, that is, not a global
    return true;
}

void ParameterVault::doAction(UIAction* a)
{
    SymbolId sid = a->symbol->id;
    if (sid == ParamTrackOverlay || sid == ParamSessionOverlay) {
        // well shucks, these are complicated
    }
    else {
        // higher levels should already have checked this
        ParameterProperties* props = a->symbol->parameterProperties.get();
        if (props != nullptr) {
            int index = props->index;
            if (index >= 0) {
                if (isValidOrdinal(props, a->value))
                  localOrdinals.set(index, a->value);
            }
        }
    }
}

bool ParameterVault::isValidOrdinal(ParameterProperties* props, int value)
{
    bool valid = false;

    switch (props->type) {
        case TypeInt: {
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
            // armegeddon
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
 * Deal with an action on trackOverlay
 * If the overlay changes, this can have widespread effects on other parameters
 * so the entire thing needs to be reflatttened.
 */
#if 0
void ParameterVault::doTrackOverlay(UIAction* a)
{
    // if the reference in the action does not resolve, leave the
    // current overlay in place
    ValueSet* overlay = trackOverlay;
    
    if (strlen(a->arguments) > 0) {
        valueSet* neu = findOverlay(parameterSets, a->arguments);
        if (neu != nullptr)
          overlay = neu;
    }
    else if (a->value > 0) {
        ValueSet* neu = findOverlay(a->value);
        if (neu != nullptr)
          overlay = neu;
    }
    else {
        // an action with no value specified means to take away the
        // current overlay
        overlay = nullptr;
    }

    if (overlay != trackOverlay) {
        // reflatten using the new overlay and the existing session

        flatten(symbols, session->ensureGlobals(), track->ensureParameters(),
                sessionOverlay, trackOverlay, flattener);

        install(flattener);
    }
}
#endif

//////////////////////////////////////////////////////////////////////
//
// Static Utilities
//
//////////////////////////////////////////////////////////////////////

/**
 * Definition for the static array
 */
juce::Array<int> ParameterVault::flattener;

/**
 * Flatten parameters from the session into an ordinal array.
 * Only parameters defined in the SymbolTable's parameter array are included
 * which is currently all of them, but could do more filtering here.
 *
 * In particular global parameters don't need to be here since tracks will
 * never have overrides for them, but it doesn't really hurt anything as long
 * as they are not used as the source for those values.
 *
 * The Session may have ValueSet entries that do not correspond to parameter
 * symbols, this should not happen, it may represent old names that were changed.
 * Do a verification pass on the Maps at the end just to make sure, but this
 * part isn't necessary.
 */
void ParameterVault::flatten(SymbolTable* symbols, ParameterSets* parameterSets,
                              Session* session, int trackNumber,
                              juce::Array<int>& result)
{
    result.clearQuick();

    Session::Track* track = session->getTrackById(trackNumber);
    if (track == nullptr) {
        Trace(1, "ParameterVault: Invalid track number %d", trackNumber);
    }
    else {
        ValueSet* defaults = session->ensureGlobals();
        ValueSet* trackValues = track->ensureParameters();

        ValueSet* sessionOverlay = findSessionOverlay(parameterSets, defaults);
        ValueSet* trackOverlay = findTrackOverlay(parameterSets, defaults, trackValues);

        juce::Array<Symbol*> parameters = symbols->getParameters();

        // usualy goofyness with sparse arrays, initialize all of them to -1 to
        // indiciate there is no value, but most of these should be replaced
        for (int i = 0 ; i < parameters.size() ; i++)
          result.add(-1);

        for (auto param : parameters) {
            ParameterProperties* props = param->parameterProperties.get();
            if (props == nullptr) {
                Trace(1, "ParameterVault: Symbol got into the parameter list without properties %s",
                      param->getName());
            }
            else if (props->type == TypeString) {
                // not many of these, but they can't have ordinals so ignore them
            }
            else if (props->type == TypeStructure) {
                // need to support these
                Trace(1, "ParameterVault: Need to support structure parameter %s",
                      param->getName());
            }
            else {
                int index = props->index;
                if (index < 0 || index >= parameters.size()) {
                    Trace(1, "ParameterVault: Symbol has an invalid index %s",
                          param->getName());
                }
                else {
                    int ord = resolveOrdinal(param, defaults, trackValues, sessionOverlay, trackOverlay);
                    result.set(index, ord);
                }
            }
        }
    }
}

/**
 * The interesting part.
 */
int ParameterVault::resolveOrdinal(Symbol* symbol, ValueSet* defaults, ValueSet* trackValues,
                                    ValueSet* sessionOverlay, ValueSet* trackOverlay)
{
    int ordinal = -1;

    ParameterProperties* props = symbol->parameterProperties.get();
    if (props == nullptr) return ordinal;
    
    // first the track overlay
    if (trackOverlay != nullptr) {
        MslValue* v = trackOverlay->get(symbol->name);
        ordinal = resolveOrdinal(symbol, props, v);
    }

    // then the track itself
    if (ordinal < 0) {
        MslValue* v = trackValues->get(symbol->name);
        ordinal = resolveOrdinal(symbol, props, v);
    }

    // then the session overlay
    if (ordinal < 0 && sessionOverlay != nullptr) {
        MslValue* v = sessionOverlay->get(symbol->name);
        ordinal = resolveOrdinal(symbol, props, v);
    }

    // and finally the session defaults
    if (ordinal < 0) {
        MslValue* v = defaults->get(symbol->name);
        ordinal = resolveOrdinal(symbol, props, v);
    }

    // for new or empty sessions, use the default from the definition
    if (ordinal < 0 && 
        (props->type == TypeInt ||
         props->type == TypeBool ||
         props->type == TypeEnum)) {
        
        ordinal = props->defaultValue;
    }

    return ordinal;
}

/**
 * This is basically what the old Enumerator did.
 * Don't trust whatever came down in the ValueSet, do range checking on it
 * before we put it in the array.
 */
int ParameterVault::resolveOrdinal(Symbol* s, ParameterProperties* props, MslValue* v)
{
    int ordinal = -1;
    if (v != nullptr) {
        switch (v->type) {
            case MslValue::Int: {
                int value = v->getInt();
                if (!isValidOrdinal(props, value))
                  Trace(1, "ParameterVault: Session value for %s out of range %d",
                        s->getName(), value);
                else
                  ordinal = value;
            }
                break;
            case MslValue::Float: {
                // really not expecting these
                Trace(1, "ParameterVault: Session value for %s was a flaot",
                      s->getName());
            }
                break;
            case MslValue::Bool: {
                // go ahead and collapse this to 0/1
                ordinal = (v->getBool() ? 1 : 0);
            }
                break;
            case MslValue::Keyword:
            case MslValue::String: {
                // accept "true" and "false" for Bool targets, otherwise
                // it needs to coerce
                if (props->type == TypeBool) {
                    if (strcmp(v->getString(), "true") == 0)
                      ordinal = 1;
                    else
                      ordinal = 0;
                }
                else if (props->type == TypeInt) {
                    // todo: validate that the string is in fact an int
                    ordinal = v->getInt();
                }
                else if (props->type == TypeEnum) {
                    // this will be left -1 if not in the enumeration
                    ordinal = props->values.indexOf(juce::String(v->getString()));
                    if (ordinal < 0)
                      Trace(1, "ParameterVault: Session value for %s was an invalid enumration %s",
                            s->getName(), v->getString());
                }
            }
                break;
            case MslValue::Enum: {
                if (props->type == TypeEnum) {
                    // these have both a name and an ordinal, I've been preferring
                    // to resolve against the name though they are supposed to match
                    int value = v->getInt();
                    if (!isValidOrdinal(props, value)) {
                        Trace(1, "ParameterVault: Session value for %s out of enumeration range %d",
                              s->getName(), value);
                        // fall back to the name since we have both
                        ordinal = props->values.indexOf(juce::String(v->getString()));
                        if (ordinal < 0)
                          Trace(1, "ParameterVault: Session value for %s was an invalid enumration %s",
                                s->getName(), v->getString());
                    }
                    else {
                        ordinal = value;
                        // this isn't necessary, but validate the name against the definition
                        // just to catch inconsistencies in the model
                        // might happen for old sessions saved before a model change

                        int other = props->values.indexOf(juce::String(v->getString()));
                        if (other < 0)
                          Trace(1, "ParameterVault: Session value for %s was an invalid enumration %s",
                                s->getName(), v->getString());
                        else if (other != ordinal) {
                            // which one is more trustworth
                            Trace(1, "ParameterVault: Session value for %s has mismatched ordinals %d %d",
                                  s->getName(), ordinal, other);
                            // in the case where you add something to the enumeration the names
                            // tend to be the same but the positions are different, so favor
                            // the name
                            ordinal = other;
                        }
                    }
                }
                else {
                    // There is an Enum value for a non-enum parameter, this is weird
                    Trace(1, "ParameterVault: Session value for %s was an enumeration",
                          s->getName());
                    ordinal = v->getInt();
                }
            }
                break;
            case MslValue::List: {
                Trace(1, "ParameterVault: Session value for %s was a List",
                      s->getName());
            }
                break;
            case MslValue::Symbol: {
                Trace(1, "ParameterVault: Session value for %s was a Symbol",
                      s->getName());
            }
                break;
        }
    }
    return ordinal;
}

ValueSet* ParameterVault::findSessionOverlay(ParameterSets* sets, ValueSet* globals)
{
    const char* ovname = globals->getString("sessionOverlay");
    return findOverlay(sets, ovname);
}

ValueSet* ParameterVault::findTrackOverlay(ParameterSets* sets, ValueSet* globals, ValueSet* track)
{
    const char* ovname = track->getString("trackOverlay");
    if (ovname == nullptr)
      ovname = globals->getString("trackOverlay");
    return findOverlay(sets, ovname);
}

ValueSet* ParameterVault::findOverlay(ParameterSets* sets, const char* ovname)
{
    ValueSet* overlay = nullptr;
    if (ovname != nullptr) {
        if (sets == nullptr)
          Trace(1, "ParameterVault: No ParameterSets defined");
        else {
            overlay = sets->find(juce::String(ovname));
            if (overlay == nullptr)
              Trace(1, "ParameterVault: Invalid parameter overlay name %s", ovname);
        }
    }
    return overlay;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
