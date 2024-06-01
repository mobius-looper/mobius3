/**
 * Simple memory allocation tracking tool till
 * I can figure out something better.
 */

extern bool MemTraceEnabled;

extern void MemTrack(void* obj, const char* className, int size);
extern void* MemNew(void* obj, const char* className, int size);
extern void MemDelete(void* obj, const char* name);

float* MemNewFloat(const char* context, int size);
char* MemCopyString(const char* context, const char* src);

#if 1


#define NEW(cls) (cls*)MemNew(new cls, #cls, sizeof(cls))

#define NEW1(cls, arg1) (cls*)MemNew(new cls(arg1), #cls, sizeof(cls))

// something weird is going on
// If I call this just "NEW" there are syntax errors on
// lines that use single arg NEW(cls), have to make the macro name different
// maybe that's just how macros work?
//#define NEW(cls, arg1, arg2) (cls*)MemNew(new cls(arg1, arg2), #cls, sizeof(cls))

#define NEW2(cls, arg1, arg2) (cls*)MemNew(new cls(arg1, arg2), #cls, sizeof(cls))

#else

#define NEW(cls) new cls

#define NEW(cls, arg1) new cls(arg1)

#define NEW(cls, arg1, arg2) new cls(arg1, arg2)

#endif
