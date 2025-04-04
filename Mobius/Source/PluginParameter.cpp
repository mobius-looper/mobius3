
#include <JuceHeader.h>

#include "util/Trace.h"
#include "util/Util.h"
#include "model/Symbol.h"
#include "model/ParameterProperties.h"
#include "model/Binding.h"
#include "model/VariableDefinition.h"

#include "PluginParameter.h"

PluginParameter::~PluginParameter()
{
    if (!installed) {
        delete intParameter;
        delete boolParameter;
        delete choiceParameter;
    }
}

PluginParameter::PluginParameter(Symbol* s, Binding* binding)
{
    symbol = s;
    
    // capture binding args
    // todo: get the complex arg string too
    setScope(binding->scope);

    ParameterProperties* props = s->parameterProperties.get();
    
    if (props != nullptr) {

        juce::String parameterId = s->getName();
        juce::String parameterName = props->displayName;
        if (parameterName.length() == 0)
          parameterName = parameterId;

        // this was missing for awhile, don't trust it
        UIParameterType type = props->type;
        if (props->values.size() > 0)
          type = TypeEnum;
        
        if (type == TypeInt) {
            min = props->low;
            // todo: if p->dynamic will have to calculate high
            max = props->high;
            intParameter = new juce::AudioParameterInt(parameterId, parameterName, min, max, 0);
        }
        else if (type == TypeBool) {
            min = 0;
            max = 1;
            boolParameter = new juce::AudioParameterBool(parameterId, parameterName, false);
        }
        else if (type == TypeEnum) {
            juce::StringArray values;
            if (props->valueLabels.size() > 0)
              values = props->valueLabels;
            else
              values = props->values;
            
            choiceParameter = new juce::AudioParameterChoice(parameterId, parameterName, values, 0);
            min = 0;
            max = values.size() - 1;
        }

    }
    else if (s->functionProperties != nullptr) {
        // Hosts have not historically had the notion of a "momentary button" parameter type
        // and neither does Juce
        // Old Mobius exposed these as booleans
        juce::String parameterId = s->name;
        juce::String parameterName = s->name;
        min = 0;
        max = 1;
        boolParameter = new juce::AudioParameterBool(parameterId, parameterName, false);
    }
    else if (s->script != nullptr) {
        // These almost always behave like functions, though there is the !continuous
        // script option that I'm not sure works
        juce::String parameterId = s->getName();
        juce::String parameterName = s->getName();
        min = 0;
        max = 1;
        boolParameter = new juce::AudioParameterBool(parameterId, parameterName, false);
    }
    else {
        // anything else to support?
        // Samples not interesting yet
        // activations however are...
        
        Trace(1, "PluginParameter: Binding to Symbol %s that wasn't a parameter or function",
              s->getName());
    }
}

PluginParameter::PluginParameter(Symbol* s, VariableDefinition* def)
{
    symbol = s;
    
    juce::var displayName = def->get("displayName");
    juce::var id = def->get("parameterId");

    // in Juce land, parameterID is an internal name
    // and "name" is the nice display name

    juce::String parameterId = s->name;
    juce::String parameterName = s->name;

    if (!id.isVoid())
      parameterId = id.toString();

    if (!displayName.isVoid())
      parameterName = displayName.toString();

    juce::String type = def->getString("type");
    
    if (type == "float") {
        // this one can have a NormalisableRange
        // not sure what I would use this for, but could come in handy
        // it's rather complicated
    }
    else if (type == "bool") {
        bool dflt = def->getBool("default");
        boolParameter = new juce::AudioParameterBool(parameterId, parameterName, dflt);
        min = 0;
        max = 1;
        last = (dflt) ? 1 : 0;
    }
    else if (type == "choice") {
        // See indexFromString lambda
        // "some hosts use this to allow users to type in a parameter"
        // final optional arg is AudioParameterChoiceAttributes which has
        // a bunch of what look like string conversion options
        juce::StringArray values {"Male", "Female", "Yes"};
        juce::String csv = def->getString("values");
        int dflt = def->getInt("default");
        if (csv.length() > 0) {
            values = juce::StringArray::fromTokens(csv, ",", "");
        }

        choiceParameter = new juce::AudioParameterChoice(parameterId, parameterName, values, dflt);
        min = 0;
        max = values.size() - 1;
        last = dflt;
    }
    else {
        // assume the usual int
        min = def->getInt("min");
        max = def->getInt("max");
        int dflt = def->getInt("default");
        intParameter = new juce::AudioParameterInt(parameterId, parameterName, min, max, dflt);
        last = dflt;
    }
}

/**
 * Deal with the silly multiple representations when we want
 * to add it to the AudioProcessor
 */
juce::AudioProcessorParameter* PluginParameter::getJuceParameter()
{
    if (intParameter != nullptr)
      return intParameter;

    if (boolParameter != nullptr)
      return boolParameter;
    
    return choiceParameter;
}

const char* PluginParameter::getScope()
{
    return scope;
}

// we've been representing this as a char array for the kernel, though
// juce::String should be fine since it is never extended
void PluginParameter::setScope(juce::String s)
{
    CopyString(s.toUTF8(), scope, sizeof(scope));
}

//////////////////////////////////////////////////////////////////////
//
// Audio Thread Accessors
//
// Methods from here down are called on the audio thread and must be
// well behaved
//
//////////////////////////////////////////////////////////////////////

/**
 * Get the current value of the parameter as an integer.
 */
int PluginParameter::getCurrent()
{
    int value = 0;

    if (intParameter != nullptr)
      value = intParameter->get();

    else if (boolParameter != nullptr)
      value = boolParameter->get();

    else if (choiceParameter != nullptr)
      value = choiceParameter->getIndex();

    return value;
}

/**
 * Should be called once at the start of an audio block to capture
 * the current value and see if it changed.
 * Returns true if a change was detected.
 * Caller may then call get() to get the captured value.
 *
 * This can only be done once so we don't have to mess with a follow-on
 * action at the end of the block to prepare for the next one.
 */
bool PluginParameter::capture()
{
    bool changed = false;
    
    int current = getCurrent();
    if (current != last) {
        last = current;
        changed = true;
    }
    
    return changed;
}

/**
 * Return the last known value, which was ordinally just captured.
 */
int PluginParameter::get()
{
    return last;
}

/**
 * Change the value of a parameter to reflect state
 * within the plugin not under the host's control.
 */
void PluginParameter::set(int neu)
{
    // what happens if this is out of range?
    // this would be an internal code error but if we don't remember
    // what the engine wanted it to be, we'll keep getting a change notice
    // every time, I guess that's one way to ensure it gets fixed
    if (neu >= min && neu <= max) {

        if (intParameter != nullptr)
          // god I hate operator overloading
          *intParameter = neu;

        else if (boolParameter != nullptr)
          *boolParameter = (neu > 0) ? true : false;

        else if (choiceParameter != nullptr)
          *choiceParameter = neu;
    
        // this one is more troublesome because if the
        // juce object rejects the value or does any sort of
        // processing on it that makes it different, we could
        // get into an endless set of change notifications every
        // audio block
        // could also do this
        // lastValue = getInt();
        last = neu;
    }
    else {
        Trace(1, "PluginParameter: Value out of range, enjoy the trace log!\n");
        last = neu;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
