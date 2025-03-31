/**
 * The style of differencing here is "effective differences".
 * Only changes that would be visible to each track are reported.
 *
 * For example, for parameter X the default value is 1
 * In a track override the value is 2.
 * The default for X is changed to 3.
 *
 * Even though X changed defaults this would not be visible in that track
 * because the override is still in place and remained 2.
 * This does not cause termination of the parameter binding.
 *
 * The bulk of the differences can be detected by iterating ovar the keys
 * defined in the default parameter set since most of them are in there.
 * Only a few flagged with "noDefault" have to be examined for differences that
 * exist only between two track override layers.
 *
 * Track counts are expected not to change, if tracks are inserted or removed in the
 * edited session then this is not smart about trying to match them up using session
 * track ids like TrackManager does when it reorganizes the track array.  It could, but
 * rearranging tracks is a pretty massive change, and it seems enough just to try to catch
 * the common case where the tracks all still line up.    Tackle that another day.
 */

#include <JuceHeader.h>

#include "util/Trace.h"
#include "util/Util.h"

#include "model/Session.h"
#include "model/ValueSet.h"
#include "model/SessionDiff.h"
#include "model/Symbol.h"

#include "Provider.h"

#include "SessionDifferencer.h"

SessionDifferencer::SessionDifferencer(Provider* p)
{
    provider = p;
}

SessionDiffs* SessionDifferencer::diff(Session* s1, Session* s2)
{
    result.reset(new SessionDiffs());

    original = s1;
    modified = s2;

    for (int i = 0 ; i < s2->getTrackCount() ; i++) {
        Session::Track* t2 = s2->getTrackByIndex(i);
        Session::Track* t1 = s1->getTrackByIndex(i);
        if (t1 == nullptr) {
            Trace(1, "SessionDifferencer: Mismatched track counts, bailing");
            break;
        }
        else {
            diff(t1->ensureParameters(), t2->ensureParameters(), i+1);
        }
    }

    return result.release();
}

void SessionDifferencer::diff(ValueSet* v1, ValueSet* v2, int track)
{
    SymbolTable* symbols = provider->getSymbols();
    ValueSet* originalDefaults = original->ensureGlobals();
    ValueSet* newDefaults = modified->ensureGlobals();
    
    // use the original default parameter set as the key guide
    juce::StringArray keys;
    original->ensureGlobals()->getKeys(keys);

    for (auto key : keys) {
        Symbol* s = symbols->find(key);
        if (s == nullptr) {
            Trace(1, "SessionDifferencer: Invalid symbol key %s", key.toUTF8());
        }
        else {
            MslValue* srcv = v1->get(key);
            if (srcv == nullptr) {
                // descend to the defaults layer
                srcv = originalDefaults->get(key);
            }
            
            MslValue* neuv = v2->get(key);
            if (neuv == nullptr) {
                neuv = newDefaults->get(key);
            }
            
            if (!isEqual(srcv, neuv)) {
                SessionDiff diff;
                diff.track = track;
                result->diffs.add(diff);
            }
        }
    }

    // todo: pick up the noDefaults
    // that will only exist in the track value sets
    // these are trackName, trackType, trackGroup, focus, trackNoReset, trackNoModify
    // none of these has any impact on behavioral parameters since they are all also
    // noBinding and could not be changed anyway

    // todo: fucking overlays
    // this throws another layer into everything and will be more common
    // with older users, need to factor those in here too
}

bool SessionDifferencer::isEqual(MslValue* v1, MslValue* v2)
{
    bool equal = false;
    if (v1 == nullptr && v2 == nullptr)
      equal = true;
    else if (v1 != nullptr && v2 != nullptr) {
        // don't need to get too fancy about type coercion, these will either both be ints
        // or both be enums
        if (v1->type == MslValue::Int) {
            equal = (v1->getInt() == v2->getInt());
        }
        else {
            // for enums, don't trust the ordinals, always go to the symbolic names
            const char* s1 = v1->getString();
            const char* s2 = v2->getString();
            equal = StringEqual(s1, s2);
        }
    }
    return equal;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
