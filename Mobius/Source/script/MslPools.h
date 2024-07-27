/**
 * A collection of object pools managed by the MslEnvironment.
 */

#pragma once

class MslPools
{
  public :

    MslPools();
    ~MslPools();

    // fill out the initial set of pooled objects
    void initialize();
    
    // called in the shell maintenance thread to replenish the pools
    // if they dip below their pool threshold
    void fluff();

    MslValue* allocValue();
    void free(MslValue* v);
    
    MslError* allocError();
    void free(MslError* v);
    
    MslResult* allocResult();
    void free(MslResult* r);

    MslSession* allocSession();
    void free(MslSession* s);

    Mslstack* allocStack();
    void free(MslStack* s);

    MslBinding* allocBinding();
    void free(MslBinding* b);

  private:

    MslValue* valuePool = nullptr;
    MslError* errorPool = nullptr;
    MslResult* resultPool = nullptr;
    
    MslSession* sessionPool = nullptr;
    Mslstack* stackPool = nullptr;
    MslBinding* bindingPool = nullptr;

};
    
