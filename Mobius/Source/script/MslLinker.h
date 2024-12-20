/**
 * Utillty class used by MslEnvironment to resolve references
 * in a compilation unit.
 */

#pragma once

class MslLinker
{
  public:

    MslLinker() {}
    ~MslLinker() {}

    void link(class MslContext* c, class MslEnvironment* env, class MslCompilation* unit);
    void checkCollisions(MslEnvironment* env, MslCompilation* comp);

  private:

    // state used during linking
    class MslContext* context = nullptr;
    class MslEnvironment* environment = nullptr;
    class MslCompilation* unit = nullptr;

    void link(class MslFunction* f);
    void link(class MslNode* node);
    void link(class MslSymbolNode* s);
    
    void addError(class MslNode* node, juce::String msg);
    void addWarning(class MslNode* node, juce::String msg);

    void checkCollisions();
    void checkCollision(juce::String name);

    void resolve(class MslSymbolNode* sym);
    void resolveLocal(class MslSymbolNode* sym);
    void resolveLocal(class MslSymbolNode* sym, class MslNode* node);
    void resolveFunctionArgument(class MslSymbolNode* sym, class MslFunctionNode* def);
    void resolveFunctionArgument(class MslSymbolNode* sym, class MslBlockNode* decl);
    void resolveEnvironment(class MslSymbolNode* sym);
    void resolveScriptArgument(class MslSymbolNode* sym);
    void resolveExternal(class MslSymbolNode* sym);
    bool isExternalKeyword(class MslSymbolNode* sym);
    void resolveExternalUsage(class MslSymbolNode* sym);
    void resolveStandard(class MslSymbolNode* sym);
    
    void compileArguments(class MslSymbolNode* sym);
    void compileArguments(class MslSymbolNode* sym, class MslBlockNode* signature);
    class MslAssignmentNode* findCallKeyword(juce::Array<class MslNode*>& callargs, juce::String name);
    class MslNode* findCallPositional(juce::Array<class MslNode*>& callargs);
    

};

