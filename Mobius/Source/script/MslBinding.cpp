/**
 * Binding between a symbolc name and a value.
 * Used by the MslSession at runtime.
 */

#include "../util/Trace.h"
#include "../util/Util.h"

#include "MslValue.h"
#include "MslBinding.h"

MslBinding::MslBinding()
{
    init();
}

MslBinding::~MslBinding()
{
    // the remainder of the list I'm on
    delete next;
    
    // not expeting to have a lingering value here, though
    // if this was an abnormal termination this might be okay?
    if (value != nullptr) {
        Trace(1, "MslBinding: Deleting lingering value");
        delete value;
    }
}

void MslBinding::init()
{
    next = nullptr;
    strcpy(name, "");
    value = nullptr;
    position = 0;
}

void MslBinding::setName(const char* s)
{
    if (s == nullptr)
      strcpy(name, "");
    else
      strncpy(name, s, sizeof(name));
}

MslBinding* MslBinding::find(const char* argName)
{
    MslBinding* found = nullptr;
    MslBinding* ptr = this;
    while (found == nullptr && ptr != nullptr) {
        if (StringEqual(argName, ptr->name))
          found = ptr;
        ptr = ptr->next;
    }
    return found;
}

MslBinding* MslBinding::find(int argPosition)
{
    MslBinding* found = nullptr;
    MslBinding* ptr = this;
    while (found == nullptr && ptr != nullptr) {
        if (argPosition == ptr->position)
          found = ptr;
        ptr = ptr->next;
    }
    return found;
}
