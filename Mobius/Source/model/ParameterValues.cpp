
#include <JuceHeader.h>

#include "ParameterValues.h"

void ParmaeterValues::configure(int max)
{
    for (int i = tracks.size() ; i < max ; i++) {
        ParameterValue v;
        v.unbound = true;
        tracks.add(v);
    }
}

int ParameterValue::getInt(int track)
{
    int value = 0;
    
    if (track >= 0 && track < tracks.size()) {
        if (!(tracks[track].unbound))
          value = tracks[track].number;
    }
    return value;
}
