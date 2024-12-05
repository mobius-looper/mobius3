/**
 * Wraps the MslVariableNode for use in Linkages
 */

#pragma once

#include "MslModel.h"
#include "MslValue.h"

/**
 * A helper class for MslVariable that represents the value
 * of a variable with track scope.  This is an optimization to avoid
 * having to allocate an array of MslValues for every possible scope which can be large
 * and the array is usually sparse.  90% of variable values are integers and only a
 * handful are strings and fewer still are the other data types MslValue supports
 *
 * The MslValue when necessary is allocated from the pool.  
 * 
 * Alternately could maintain these on a list but there can be many tracks for some
 * users and linear searches become tiresome.
 */
class MslScopedValue
{
  public:

    // true if the value is bound
    bool bound = false;

    // the ordinal or integer value which is by far the most common
    // booleans will go here too
    int ival = 0;

    // if the variable is assigned a string value, then a full MslValue is
    // allocated from the pool at that time
    MslValue* value = nullptr;
};

class MslVariable
{
    friend class MslParser;
    
  public:

    static const int MaxScope = 256;

    MslVariable();
    MslVariable(class MslPools* pools);
    void setPool(class MslPools* pools);
    ~MslVariable();
    
    // reference name of the variable
    // same as node->name
    juce::String name;
    
    bool isExport() {
        return (node != nullptr) ? node->keywordExport : false;
    }
    bool isPublic() {
        return (node != nullptr) ? node->keywordPublic : false;
    }
    bool isGlobal() {
        return (node != nullptr) ? node->keywordGlobal : false;
    }
    bool isScoped() {
        return (node != nullptr) ? node->keywordScope : false;
    }

    bool isBound(int scopeId);
    void unbind(int scopeId);
    void getValue(int scopeId, MslValue* dest);
    void setValue(int scopeId, MslValue* src);

    // older variants that only work with non-scoped values
    bool isBoundUnscoped();
    void unbind();
    void getValueUnscoped(MslValue* dest);
    void setValueUnscoped(MslValue* src);
    
  protected:
    
    void setNode(MslVariableNode* v);
    class MslVariableNode* getNode();

  private:

    class MslPools* pool = nullptr;
    
    // unlike MslFunction, this is not a unique_ptr since
    // we don't remove the node from the parse tree
    class MslVariableNode* node = nullptr;

    // the current static value
    MslValue value;

    // scope-specific values
    juce::Array<MslScopedValue> scopeValues;

    // true once the variable has been given a value, including null
    // for scoped values, this becomes the default value 
    bool bound = false;


    void unbind(MslScopedValue& value);
    
};


    
