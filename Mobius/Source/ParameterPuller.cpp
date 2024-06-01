
#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/Parameter.h"

#include "ParameterPuller.h"

ParameterPuller::ParameterPuller()
{
}

ParameterPuller::~ParameterPuller()
{
}

void ParameterPuller::pull()
{
    juce::String buf;
    
    for (int i = 0 ; i < Parameter::Parameters.size() ; i++) {
        Parameter* p = Parameter::Parameters[i];
        trace("Parameter %s\n", p->getName());

        buf += "<Parameter name='";
        buf += p->getName();
        buf += "'";

        if (p->type == TYPE_BOOLEAN) {
            buf += " type='bool'";
        }
        else if (p->type == TYPE_STRING) {
            buf += " type='string'";
        }

        if (p->multi){
            buf += " multi='true'";
        }

        if (p->scope == PARAM_SCOPE_PRESET) {
            buf += " scope='preset'";
        }
        else if (p->scope == PARAM_SCOPE_SETUP) {
            buf += " scope='setup'";
        }
        else if (p->scope == PARAM_SCOPE_TRACK) {
            buf += " scope='track'";
        }
        else if (p->scope == PARAM_SCOPE_GLOBAL) {
            buf += " scope='global'";
        }
        
        if (p->low > 0) {
            buf += " low='";
            buf += juce::String(p->low);
            buf += "'";
        }

        if (p->high > 0) {
            buf += " high='";
            buf += juce::String(p->high);
            buf += "'";
        }

        if (p->values != nullptr) {
            buf += " values='";
            for (int i = 0 ; p->values[i] != nullptr ; i++) {
                if (i > 0)
                  buf += ",";
                buf += p->values[i];
            }
            buf += "'";
        }
        
        if (p->valueLabels != nullptr) {
            buf += " valueLabels='";
            for (int i = 0 ; p->valueLabels[i] != nullptr ; i++) {
                if (i > 0)
                  buf += ",";
                buf += p->valueLabels[i];
            }
            buf += "'";
        }
        
        if (p->defaultValue > 0) {
            buf += " defaultValue='";
            buf += juce::String(p->defaultValue);
            buf += "'";
        }

        buf += " options='";

        if (p->bindable)
          buf += "bindable,";
        if (p->control)
          buf += "control,";
        if (p->juceValues)
          buf += "juceValues,";
        if (p->zeroCenter)
          buf += "zeroCenter,";
        if (p->dynamic)
          buf += "dynamic,";
        if (p->transient)
          buf += "runtime,";
        if (p->resettable)
          buf += "resettable,";
        if (p->scheduled)
          buf += "scheduled,";

        buf += "'/>\n";

        juce::File file ("c:/dev/jucetest/UI/Source/pulled.xml");
        juce::Result result = file.create();
        if (result.failed()) {
            std::cout << "File creation failed\n";
        }
        else {
            file.replaceWithText(buf);
        }
    }
    
}
