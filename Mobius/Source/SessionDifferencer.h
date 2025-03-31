/**
 * Utility to compare two Sessions and determine what changed.
 */

#pragma once

class SessionDifferencer
{
  public:

    SessionDifferencer(Provider* p);
    
    // differences are allocated dynamically and must be deleted
    class SessionDiffs* diff(class Session* original, class Session* modified);

  private:

    Provider* provider = nullptr;
    Session* original = nullptr;
    Session* modified = nullptr;
    std::unique_ptr<SessionDiffs> result = nullptr;
    
    void diff(class ValueSet* src, class ValueSet* neu, int track);
    bool isEqual(class MslValue* v1, class MslValue* v2);

};



