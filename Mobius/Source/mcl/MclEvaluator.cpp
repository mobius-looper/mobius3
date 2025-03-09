
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
    for (auto obj : script->objects) {
        
        if (obj->type == MclObjectScope::Session)
          evalSession(obj);
        else
          evalOverlay(obj);

        if (!hasErrors())
          addResult(obj);
        else
          break;
    }
}

void MclEvaluator::addResult(MclObjectScope* obj)
{
    juce::String msg = "Updated ";
    if (obj->type == MclObjectScope::Overlay)
      msg += "overlay ";
    else
      msg += "session ";
    msg += obj->name;
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

void MclEvaluator::evalSession(MclObjectScope* obj)
{
    (void)obj;
    addError("I'm broken");
}

void MclEvaluator::evalOverlay(MclObjectScope* obj)
{
    ParameterSets* overlays = provider->getParameterSets();
    ValueSet* master = overlays->find(obj->name);

    // make a copy for editing so we can bail if we hit an error
    ValueSet* target = nullptr;
    if (master == nullptr) {
        // todo: need some governors on creation
        target = new ValueSet();
        target->name = obj->name;
    }
    else {
        target = new ValueSet(master);
    }

    // there are no sub-scopes in overlays yet so just merge them all together
    for (auto scope : obj->scopedAssignments) {
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
    else if (ass->symbol == nullptr) {
        addError("Missing symbol for " + ass->name);
    }
    else if (ass->symbol->parameterProperties == nullptr) {
        addError("Symbol not a parameter " + ass->name);
    }
    else {
        ParameterProperties* props = ass->symbol->parameterProperties.get();
        MslValue v;
        if (props->type == TypeInt) {
            // todo: this won't detect any integer literal syntax errors
            v.setInt(ass->svalue.getIntValue());
        }
        else if (props->type == TypeBool) {
            if (ass->svalue == "true") {
                v.setBool(true);
            }
            else if (ass->svalue != "false") {
                // assume it's an int
                v.setBool(ass->svalue.getIntValue() != 0);
            }
        }
        else if (props->type == TypeString) {
            v.setJString(ass->svalue);
        }
        else if (props->type == TypeStructure) {
            // todo: name validation
            v.setJString(ass->svalue);
        }
        else if (props->type == TypeEnum) {
            int ordinal = props->getEnumOrdinal(ass->svalue.toUTF8());
            if (ordinal < 0)
              addError("Invalid enumeration value for " + ass->name + ": " + ass->svalue);
            else
              v.setEnum(ass->svalue.toUTF8(), ordinal);
        }
        else {
            // you added a new type, didn't you
            addError("Unsupported parameter type on " + ass->symbol->name);
        }

        if (!v.isNull())
          dest->set(ass->symbol->name, v);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
