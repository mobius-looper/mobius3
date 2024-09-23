
#include <JuceHeader.h>

#include "ValueSet.h"
#include "Session.h"

Session::Session()
{
}

Session::~Session()
{
}

void Session::Session(Session* src)
{
    for (auto track : src->tracks) {
        tracks.add(new Track(track));
    }

    ValueSet* g = &(src->globals);
    globals = new ValueSet(g);
}

void Session::Track::Track(Session::Track* src)
{
    type = src->type;
    name = src->name;
    parameters = new ValueSet(&(src->parameters));
}
