
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/Symbol.h"
#include "../model/ParameterProperties.h"
#include "../model/ParameterSets.h"
#include "../model/ValueSet.h"
#include "../Provider.h"

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
    juce::String msg = "Updated ";
    if (section->type == MclSection::Overlay)
      msg += "overlay ";
    else
      msg += "session ";
    msg += section->name;
    result->messages.add(msg);
}

void MclEvaluator::addError(juce::String err)
{
    // todo: some context
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
    (void)section;
    addError("I'm broken");
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

            evalAssignment(ass, target);
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

void MclEvaluator::evalAssignment(MclAssignment* ass, ValueSet* dest)
{
    if (ass->remove) {
        // !! if this is trackName it won't work since that isn't in the ValueSet
        // will need to pass down the Session::Track
        dest->remove(ass->name);
    }
    else {
        dest->set(ass->name, ass->value);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Bindings
//
//////////////////////////////////////////////////////////////////////

void MclEvaluator::evalBinding(MclSection* section)
{
    (void)section;
    Trace(1, "MclEvaluator::evalBinding Not expecting this yet");
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
