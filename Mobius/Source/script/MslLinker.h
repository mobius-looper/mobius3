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

    void link(MslContext* c, class MslEnvironment* env, class MslCompilation* unit);

  private:

    // state used during linking
    class MslContext* context = nullptr;
    class MslEnvironment* environment = nullptr;
    class MslCompilation* unit = nullptr; 

    // recursively link a compiled node
    void link(class MslNode* node);
    void link(Symbol* s);
    

};

