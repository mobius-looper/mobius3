
#include "../../util/Trace.h"
#include "../../util/Util.h"

#include "Mem.h"

// global variable to control whether we trace or not,
// set when we're processing blocks in the audio thread
bool MemTraceEnabled = false;
bool MemForceTrace = false;

void MemTrack(void* obj, const char* className, int size)
{
    if (MemTraceEnabled || MemForceTrace) {
        // ugh, Trace doesn't support 64 bit pointers
        char buf[1024];
        snprintf(buf, sizeof(buf), "Memory: Allocated %s size %d %p\n", className, size, obj);
        Trace(2, buf);
    }
}

void* MemNew(void* obj, const char* className, int size)
{
    if (MemTraceEnabled || MemForceTrace) {
        // ugh, Trace doesn't support 64 bit pointers
        char buf[1024];
        snprintf(buf, sizeof(buf), "Memory: Allocated %s size %d %p\n", className, size, obj);
        Trace(2, buf);
    }
    return obj;
}

float* MemNewFloat(const char* context, int size)
{
    float* buffer = new float[size];
    if (MemTraceEnabled || MemForceTrace) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "Memory: Allocated float buffer for %s size %d %p\n",
                context, (int)(size * sizeof(float)), buffer);
        Trace(2, buf);
    }
    return buffer;
}

char* MemCopyString(const char* context, const char* src)
{
    char* copy = CopyString(src);
    if (copy != nullptr && (MemTraceEnabled || MemForceTrace)) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "Memory: CopyString for %s size %d %p\n",
                context, (int)(strlen(copy) + 1), copy);
        Trace(2, buf);
    }
    return copy;
}

void MemDelete(void* obj, const char* varName)
{
    if (MemTraceEnabled || MemForceTrace) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "Memmory: Deleting %s %p\n", varName, obj);
        Trace(2, buf);
    }
}
