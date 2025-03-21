
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/Symbol.h"
#include "../model/ParameterProperties.h"
#include "../model/ParameterSets.h"
#include "../model/ValueSet.h"
#include "../model/Session.h"
#include "../model/old/MobiusConfig.h"
#include "../model/old/Structure.h"
#include "../model/old/OldBinding.h"
#include "../Provider.h"
#include "../Producer.h"

#include "MclModel.h"
#include "MclResult.h"
#include "MclEvaluator.h"

MclEvaluator::MclEvaluator(Provider* p)
{
    provider = p;
}

MclEvaluator::~MclEvaluator()
{
}

void MclEvaluator::eval(MclScript* script, MclResult& userResult)
{
    result = &userResult;

    // todo: here is where we could try to be smart about merging multiple
    // units for the same object into one, but it doesn't really matter
    // might save some file updates but not much
    for (auto section : script->sections) {

        switch (section->type) {

            case MclSection::Session:
                evalSession(section);
                break;

            case MclSection::Overlay:
                evalOverlay(section);
                break;

            case MclSection::Binding:
                evalBinding(section);
                break;

            default:
                addError("Unhandled section type");
                break;
        }

        if (!hasErrors())
          addResult(section);
        else
          break;
    }
}

void MclEvaluator::addResult(MclSection* section)
{
    juce::String msg;
    switch (section->type) {
        case MclSection::Session:
            msg += "Session ";
            break;
        case MclSection::Overlay:
            msg += "Overlay ";
            break;
        case MclSection::Binding:
            msg += "Binding Set ";
            break;
    }
    msg += section->name;
    result->messages.add(msg);

    int changes = 0;
    if (section->additions > 0) {
        juce::String details = juce::String(section->additions) + " additions";
        result->messages.add(details);
        changes++;
    }
    
    if (section->modifications > 0) {
        juce::String details = juce::String(section->modifications) + " modifications";
        result->messages.add(details);
        changes++;
    }
    
    if (section->removals > 0) {
        juce::String details = juce::String(section->removals) + " removals";
        result->messages.add(details);
        changes++;
    }

    if (changes == 0)
      result->messages.add("No changes needed to be saved");
    else {
        // is this at all useful?
        if (section->ignores > 0) {
            juce::String details = juce::String(section->ignores) + " ignored";
            result->messages.add(details);
        }
    }
}

void MclEvaluator::addError(juce::String err)
{
    // todo: some context
    result->errors.add(err);
}

void MclEvaluator::addErrors(juce::StringArray& errors)
{
    for (auto err : errors)
      result->errors.add(err);
}

bool MclEvaluator::hasErrors()
{
    return result->errors.size() > 0;
}

//////////////////////////////////////////////////////////////////////
//
// Sessions and Overlays
//
//////////////////////////////////////////////////////////////////////

void MclEvaluator::evalSession(MclSection* section)
{
    Session* session = nullptr;
    bool offline = false;
    Session* current = provider->getSession();
    
    if (section->name.length() == 0 ||
        section->name == current->getName() ||
        section->name == "active") {

        // doing this on the live session
        // what SessionEditor does is save to a copy of the Session then
        // replace the ValueSets in the master session
        // since we do have a few failure conditions here, consider that
        session = current;

        // if we defaulted to the active session
        // put the name in the section so the results assembler knows we went there
        section->name = session->getName();
    }
    else {
        offline = true;
        Producer* pro = provider->getProducer();
        session = pro->readSession(section->name);
        if (session == nullptr) {
            Producer::Result pres = pro->validateSessionName(section->name);
            if (pres.errors.size() > 0) {
                addErrors(pres.errors);
            }
            else {
                session = new Session();
                session->setName(section->name);
            }
        }
    }

    if (session != nullptr) {
        
        for (auto scope : section->scopes) {

            ValueSet* dest = nullptr;
            Session::Track* track = nullptr;
            
            if (scope->scope == 0) {
                dest = session->ensureGlobals();
            }
            else {
                track = session->getTrackById(scope->scope);
                if (track == nullptr) {
                    // going to need some options to avoid creating new tracks if they
                    // enter some bonkers numbers
                    if (scope->scope > (session->getTrackCount() + 1))
                      addError(juce::String("Track number out of range: ") + juce::String(scope->scope));
                    else {
                        // it defaults to audio which may be changed later
                        track = new Session::Track();
                        session->add(track);
                    }
                }
                if (track != nullptr)
                  dest = track->ensureParameters();
            }

            if (dest != nullptr) {
                for (auto ass : scope->assignments) {

                    // this one has a special place
                    if (ass->name == "trackName") {
                        evalTrackName(section, session, track, ass);
                    }
                    else if (ass->name == "trackType") {
                        // also special
                        evalTrackType(section, session, track, ass);
                    }
                    else if (ass->scope == 0) {
                        if (ass->remove && scope->scope == 0) {
                            // can't remove a global
                            addError(juce::String("Default parameter ") + ass->name + " may not be removed");
                        }
                        else {
                            evalAssignment(section, ass, dest);
                        }
                    }
                    else {
                        // scope assignments are intended for global scope
                        // they're not prevented in track scope but it doesn't
                        // make sense
                        if (scope->scope != 0 && scope->scope != ass->scope) {
                            addError("Scoped assignments not allowed when already within a track scope");
                        }
                        else {
                            evalScopedAssignment(section, session, ass);
                        }
                            
                    }
                    
                    if (hasErrors()) break;
                }
            }
            if (hasErrors()) break;
        }
    }

    if (!hasErrors() && session != nullptr && 
        (section->additions > 0 || section->modifications > 0 || section->removals > 0)) {
        if (!offline) {
            provider->mclSessionUpdated();
        }
        else {
            Producer* pro = provider->getProducer();
            Producer::Result pres = pro->writeSession(session);
            addErrors(pres.errors);
        }
    }

    if (offline)
      delete session;
}

void MclEvaluator::evalOverlay(MclSection* section)
{
    ParameterSets* overlays = provider->getParameterSets();
    ValueSet* master = overlays->find(section->name);

    // make a copy for editing so we can bail if we hit an error
    ValueSet* target = nullptr;
    if (master == nullptr) {
        // todo: need some governors on creation
        target = new ValueSet();
        target->name = section->name;
    }
    else {
        target = new ValueSet(master);
    }

    // there are no sub-scopes in overlays yet so just merge them all together
    for (auto scope : section->scopes) {
        for (auto ass : scope->assignments) {

            if (ass->scope == 0) {
                evalAssignment(section, ass, target);
            }
            else {
                // sub scopes within overlays are not supported yet, if ever
                // should have been caught in the parser
                addError(juce::String("Scoped assignments not allowed within overlays"));
            }
            if (hasErrors()) break;
        }
        if (hasErrors()) break;
    }

    if (!hasErrors()) {
        overlays->replace(target);
        provider->updateParameterSets();
    }
    else {
        delete target;
    }
}

void MclEvaluator::evalAssignment(MclSection* section, MclAssignment* ass, ValueSet* dest)
{
    MslValue* existing = dest->get(ass->name);
    
    if (ass->remove) {
        if (existing != nullptr) {
            // !! if this is trackName it won't work since that isn't in the ValueSet
            // will need to pass down the Session::Track
            dest->remove(ass->name);
            section->removals++;
        }
        else {
            // technically we should have an ignored count if they asked
            // to remove something that wasn't there
        }
    }
    else {
        dest->set(ass->name, ass->value);
        if (existing != nullptr)
          section->modifications++;
        else
          section->additions++;
    }
}

/**
 * This has similar targeting logic as evalSession does when handling MclScope declarations
 * but here we can't auto-create tracks if they reference a number out of range.
 * Keep them distinct.
 */
void MclEvaluator::evalScopedAssignment(MclSection* section, Session* session, MclAssignment* ass)
{
    if (section->type == MclSection::Session && ass->scope == 0 && ass->remove == true) {
        addError(juce::String("Default parameter ") + ass->name + " may not be removed");
    }
    else {
        ValueSet* dest = nullptr;
        if (ass->scope == 0) {
            // they bothered with "0:foo" which is unnecessary but allowed
            dest = session->ensureGlobals();
        }
        else {
            Session::Track* track = session->getTrackById(ass->scope);
            if (track != nullptr)
              dest = track->ensureParameters();
            else {
                // the number used is not valid
                // there isn't a way to define new tracks with MCL
                // would be nice but it requires the track type
                // explore
                addError(juce::String("Track number out of range: ") + juce::String(ass->scope));
            }
        }

        if (dest != nullptr)
          evalAssignment(section, ass, dest);
    }
}

/**
 * Track name isn't in the ValueSet, it is stored as a top-level properti
 * of the Session::Track object for easier searching.
 */
void MclEvaluator::evalTrackName(MclSection* section, Session* session, Session::Track* track, MclAssignment* ass)
{
    // we bypass normal processing for trackName early, so need to handle
    // line scope here
    if (ass->scope > 0) {
        track = session->getTrackById(ass->scope);
        if (track == nullptr) {
            addError(juce::String("Track number out of range: ") + juce::String(ass->scope));
        }
    }
    else if (track == nullptr) {
        // parser should have caught this
        addError(juce::String("trackName is not a default parameter"));
    }

    if (track != nullptr) {
        if (ass->remove) {
            if (track->name.length() == 0)
              section->ignores++;
            else {
                track->name = "";
                section->removals++;
            }
        }
        else {
            // really need some name constraints enforcement somewhere
            track->name = ass->value.getString();
            section->modifications++;
        }
    }
}

/**
 * Track type also isn't in the ValueSet.
 * Changing types is potentially dangerous, may want more safeguards around this.
 */
void MclEvaluator::evalTrackType(MclSection* section, Session* session, Session::Track* track, MclAssignment* ass)
{
    // woah, scope prefixes seem really weird for track types, I'm starting to hate them
    if (ass->scope > 0) {
        track = session->getTrackById(ass->scope);
        if (track == nullptr) {
            addError(juce::String("Track number out of range: ") + juce::String(ass->scope));
        }
    }
    else if (track == nullptr) {
        addError(juce::String("trackName is not a default parameter"));
    }

    if (track != nullptr) {
        if (ass->remove) {
            // you can't take the type away, could error or just ignore it
            section->ignores++;
        }
        else {
            juce::String typeName = juce::String(ass->value.getString());
            if (typeName.equalsIgnoreCase("audio")) {
                if (track->type == Session::TypeAudio)
                  section->ignores++;
                else {
                    // changing from MIDI to Audio can only happen if this was
                    // an existing track, it is unusual to change types, warn for a bit
                    Trace(2, "MclEvaluator: Warning: Changing track type");
                    track->type = Session::TypeAudio;
                }
            }
            else if (typeName.equalsIgnoreCase("midi")) {
                if (track->type == Session::TypeMidi)
                  section->ignores++;
                else {
                    // since the construction default is Audio we can't really warn here
                    // about changing types without knowing whether we just now created
                    // this track or not
                    track->type = Session::TypeMidi;
                    section->modifications++;
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Bindings
//
//////////////////////////////////////////////////////////////////////

/**
 * When merging into an existing BindingSet, matching is by trigger only.
 * This means that you can't have more than one binding on the same trigger
 * which was allowed in the past and could potentially be allowed now, but it
 * results in instability in behavior, and is not recommended.
 * It also makes matching for things like this fuzzy, when are you editing
 * something vs. creating a second binding?  Punt
 */
void MclEvaluator::evalBinding(MclSection* section)
{
    MobiusConfig* config = provider->getOldMobiusConfig();
    OldBindingSet* sets = config->getBindingSets();

    // fuck it, we'll do it live
    OldBindingSet* target = nullptr;
    OldBindingSet* existing = (OldBindingSet*)Structure::find(sets, section->name.toUTF8());
    if (existing == nullptr) {
        // so there isn't a way to do rename from MCL which should be added at some point
        target = new OldBindingSet();
        target->setName(section->name.toUTF8());
        target->setOverlay(section->bindingOverlay);
        config->addBindingSet(target);
    }
    else {
        target = existing;
        if (section->bindingOverlay)
          target->setOverlay(true);
        else if (section->bindingNoOverlay)
          target->setOverlay(false);
    }
    
    while (section->bindings.size() > 0) {
        OldBinding* neu = section->bindings.removeAndReturn(0);

        OldBinding* matching = nullptr;
        for (OldBinding* b = target->getBindings() ; b != nullptr ; b = b->getNext()) {
            if (b->trigger == neu->trigger &&
                b->triggerValue == neu->triggerValue &&
                b->midiChannel == neu->midiChannel &&
                b->release == neu->release) {
                matching = b;
                break;
            }
        }

        // todo: there is no way to ask that a binding be removed one at a time
        // without replacing the entire BindingSet, would be nice
        
        if (matching != nullptr) {
            matching->setSymbolName(neu->getSymbolName());
            matching->setArguments(neu->getArguments());
            matching->setScope(neu->getScope());
            delete neu;
            // I guess technically we should see if there were in fact any changes
            // and bump the ignore count
            section->modifications++;
        }
        else {
            // insert it at the beginning of the list or append it
            // if you insert at the beginning this on BindingSet::setBindings NOT
            // to delete the existing list, which it does but is inconsistent with the way most
            // of the old list setters work
            bool append = true;
            if (!append) {
                neu->setNext(target->getBindings());
                target->setBindings(neu);
            }
            else {
                target->addBinding(neu);
            }
            section->additions++;
        }
    }

    // bindingEditorSave requires a new list and will delete the old one
    // need to get this shit out of MobiusConfig
    provider->mclMobiusConfigUpdated();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
