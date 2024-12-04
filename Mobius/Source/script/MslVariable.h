/**
 * Wraps the MslVariableNode for use in Linkages
 */

#pragma once

#include "MslModel.h"

class MslVariable
{
    friend class MslParser;
    
  public:

    MslVariable() {}
    ~MslVariable() {}

    // reference name of the function
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

    juce::String getName() {
        return (node != nullptr) ? node->name : "";
    }

    bool isBound();
    void unbind();
    MslValue* getValue();
    void setValue(MslValue* v);
    
  protected:
    
    class MslVariableNode* getNode() {
        return node;
    }

    void setNode(MslVariableNode* v) {
        node = v;
    }

  private:

    // unlike MslFunction, this is not a unique_ptr since
    // we don't remove the node from the parse tree
    class MslVariableNode* node = nullptr;

    // the current static value
    MslValue value;

    // true once the variable has been given a value, including null
    bool bound = false;
};


    
