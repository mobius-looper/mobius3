/**
 * Class encapsulating management of plugin parameters for Supervisor.
 *
 * Where this lives needs thought since it will most often be required
 * by MobiusKernel when processing the audio block.   Currently MobiusKernel
 * will obtain a reference to this object through the MobiusContainer
 * to iterate over the PluginParamers.
 *
 * Since this can happen in the audio thread great care must be taken
 * when modifying the PluginParameter list.  Basically it can't be done
 * right now without reloading the plugin.
 */

#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/MobiusConfig.h"
#include "model/Binding.h"
#include "model/VariableDefinition.h"
#include "model/Symbol.h"

#include "Supervisor.h"
#include "PluginParameter.h"
#include "Parametizer.h"

Parametizer::Parametizer(Supervisor* super)
{
    supervisor = super;
}

Parametizer::~Parametizer()
{
}

void Parametizer::initialize()
{
    Trace(2, "Parametizer::initialize\n");

    int sustainId = 1;
    
    // start with Bindings
    MobiusConfig* mconfig = supervisor->getMobiusConfig();
    BindingSet* bindings = mconfig->getBindingSets();
    Binding* binding = bindings->getBindings();
    while (binding != nullptr) {
        if (binding->trigger == TriggerHost) {
            Symbol* s = supervisor->getSymbols()->intern(binding->getSymbolName());
            PluginParameter* p = new PluginParameter(s, binding);
            // we work top down from the PluginParameter to the Symbol
            // so we don't need to hang the PluginParameter on the Symbol
            // though I suppose we could if that were interesting
            
            if (p->getJuceParameter() == nullptr) {
                // hmm: There was an error in the definition that
                // prevented the construction of a proper AudioProcessorParameter
                // interface is awkward, we have to create it before we know that
                // PluginParameter could trace this too
                Trace(1, "Parametizer: Ignoring incomplete parameter binding for %s\n",
                      s->getName());
                delete p;
            }
            else {
                p->sustainId = sustainId++;
                parameters.add(p);
            }
        }
        binding = binding->getNext();
    }

    // for testing I like to allow VariableDefinitions with automatable=true
    // without requiring explicit Bindings
    // todo: rather than iterating over the VariableManager list, could also
    // iterate over Symbols looking foro the ones that have a VariableDefinition
    // todo: if a VariableDefinition and a Binding have the same name we'll get
    // duplicates unless we put the previous PluginParameter as a property of the Symbol
    VariableManager* vm = supervisor->getVariableManager();
    VariableDefinitionSet* vars = vm->getVariables();
    if (vars != nullptr) {
        for (auto var : vars->variables) {
            if (var->getBool("automatable")) {
                Symbol* s = supervisor->getSymbols()->intern(var->name);
                PluginParameter* p = new PluginParameter(s, var);
                if (p->getJuceParameter() == nullptr) {
                    Trace(1, "Parametizer: Ignoring incomplete parameter definition %s\n",
                          var->name.toUTF8());
                    delete p;
                }
                else {
                    parameters.add(p);
                }
            }
        }
    }

}

/**
 * Install the PluginParameters we previously assembled.
 * Split out from initialize so we can test it standalone,
 * but here we must be in a plugin,
 */
void Parametizer::install()
{
    Trace(2, "Parametizer::install\n");
    
    // we're going to need this a lot right?  maybe ust pass that
    // to the constructor
    juce::AudioProcessor* ap = supervisor->getAudioProcessor();
    if (ap == nullptr) {
        Trace(1, "Parametizer::install You are not a plugin\n");
    }
    else {
        for (auto param : parameters) {
            juce::AudioProcessorParameter* app = param->getJuceParameter();
            if (app != nullptr) {
                ap->addParameter(app);
                param->installed = true;
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
