/**
 * Utility to compare two Sessions and determine what changed.
 */

#pragma once

class SessionDifferencer
{
  public:
    
    // differences are allocated dynamically and must be deleted
    static class SessionDiffs* diff(class Provider* p, class Session* original, class Session* modified);

  private:
    
    static void diffValueSet(class Provider* p, class SessionDiffs* diffs,
                             class ValueSet* src, class ValueSet* neu);
};



