/**
 * The association of a name with a value within a block during evaluation.
 * Not to be confused with model/Binding which is the association of a trigger
 * with an action.  This exists only with in the MSL interpreter.
 *
 * These are created as values are assigned to MslVars, and to represent the argument
 * list passed to procs in a call.  Because they are used at runtime, they must
 * use the non-allocation MslValue model and need to be pooled.
 *
 * NOTE: We could skip having another pooled objects by giving MslValue a name
 * but I'm liking keeping the models distinct.
 *
 */

#pragma once

class MslBinding
{
  public:

    MslBinding();
    ~MslBinding();
    void init();

    // bindings are maintained on a list within an MslStack frame
    MslBinding* next = nullptr;

    // the name of the binding is taken from the symbol used in an assignment
    // of Var declaration
    static const int MaxBindingName = 128;
    char name[MaxBindingName];
    
    // bindings usually have a value, though it is not set until an assignment
    // node is reached during evaluation
    class MslValue* value = nullptr;

    // for proc arguments, the position of this argument in the declaration
    // used to resolve alternate $x references rather than name references
    int position = 0;

    // todo: if this overloads an external Symbol, will need information
    // about save/restore state

    void setName(const char* s);
    MslBinding* copy(MslBinding* src);
    MslBinding* find(const char* s);
    MslBinding* find(int position);
    
};

