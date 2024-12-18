/**
 * An object representing a callable thing within the MSL environment.
 *
 * public fields are accessible by the application
 * protected fields are only accessible within the environment
 *
 * There are three ways to build an MslFunction
 *
 *    - wrapping an MslFunctionNode found in the parse tree
 *    - wrapping the body block of a script file
 *    - wrapping the initialization block of a script file
 *
 * The first case contains both the declaration and body blocks.
 * The second two have a body block extracted from the parse tree
 * but the declaration block is either defined in a special way or
 * missing.
 *
 * It might be easier to synthesize an MslFunctionNode around the second
 * two so they can all three be handled the same.
 * If we did that that then the utility of MslFunction kind of goes away
 * as we could just represent them everywhere with MslFunctionNode.
 *
 * But I would like to maybe compile a sanitized version of the raw declaration
 * block and that would clutter up the FunctionNode.
 *
 */

#pragma once

#include "../util/Trace.h"

// need MslFunctionNode
#include "MslModel.h"

class MslFunction
{
    // why again is the body protected if we need this may friends?
    friend class MslEnvironment;
    friend class MslConductor;
    friend class MslParser;
    friend class MslLinker;
    friend class MobiusConsole;
    friend class MslSession;
    friend class MslResolution;
    
  public:

    MslFunction() {}
    ~MslFunction() {}

    // reference name of the function
    juce::String name;

    // various declaration results
    // todo: lots more here
    bool sustainable = false;

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

  protected:

    class MslBlockNode* getBody() {
        MslBlockNode* retval = nullptr;
        if (body != nullptr) {
            retval = body.get();
        }
        else if (node != nullptr) {
            retval = node->getBody();
        }
        return retval;
    }

    void setNode(MslFunctionNode* n) {
        if (body != nullptr)
          Trace(1, "MslFunction: Conflicting body sources");
        node.reset(n);
    }

    void setBody(MslBlockNode* b) {
        if (node != nullptr)
          Trace(1, "MslFunction: Conflicting body sources");
        body.reset(b);
    }
    
    // special for MslParser
    class MslBlockNode* releaseBody() {
        return (body != nullptr) ? body.release() : nullptr;
    }

    class MslBlockNode* getDeclaration() {
        MslBlockNode* retval = nullptr;
        if (declaration != nullptr)
          retval = declaration.get();
        else if (node != nullptr)
          retval = node->getDeclaration();
        return retval;
    }

    void setDeclaration(MslBlockNode* b) {
        declaration.reset(b);
    }

  private:

    // set when this was derived from an MslFunctionNode
    // e.g. a "function foo" at the top level of the script body
    std::unique_ptr<class MslFunctionNode> node;

    // set when this was derived from an init block or the top level
    // script body block
    std::unique_ptr<class MslBlockNode> body;

    // set when this was derived from the top level script body block
    // and there was a declaration for the script signature
    std::unique_ptr<class MslBlockNode> declaration;

};


    
