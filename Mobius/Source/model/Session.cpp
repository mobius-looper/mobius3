
#include <JuceHeader.h>

#include "ValueSet.h"
#include "Session.h"

Session::Session()
{
}

Session::~Session()
{
}

Session::Session(Session* src)
{
    for (auto track : src->tracks) {
        tracks.add(new Track(track));
    }

    globals.reset(new ValueSet(src->globals.get()));
}

Session::Track::Track(Session::Track* src)
{
    type = src->type;
    name = src->name;
    parameters.reset(new ValueSet(src->parameters.get()));
}
