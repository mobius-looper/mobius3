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
    void link(class MslSymbol* s);
    
    void addError(class MslSymbol* sym, juce::String msg);
    void addWarning(class MslSymbol* sym, juce::String msg);

    void checkCollisions();
    void checkCollision(juce::String name);

    void resolve(class MslSymbol* sym);
    void resolveLocal(class MslSymbol* sym);
    void resolveLocal(class MslSymbol* sym, class MslNode* node);
    void resolveEnvironment(class MslSymbol* sym);
    void resolveExternal(class MslSymbol* sym);

    void compileArguments(class MslSymbol* sym);
    void compleArguments(class MslSymbol* sym, class MslBlock* signature);
    class MslAssignment* findCallKeyword(juce::Array<class MslNode*>& callargs, juce::String name);
    class MslNode* findCallPositional(juce::Array<class MslNode*>& callargs);
    

};

