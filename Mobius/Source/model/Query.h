/**
 * Object describing a request to access the value of a parameter or
 * other abstract named value within the system.
 *
 * Conceptually simular to UIAction which changes the value of a parameter,
 * a Query returns the current value.
 *
 * It replaces the older Export concept.
 * 
 */

#pragma once

class Query
{
  public:

    Query() {}
    ~Query() {}
    
    Query(class Symbol* s) {
        symbol = s;
    }

    /**
     * Symbol representing the target of the query.  This is usually
     * a symbol associated with a parameter, but can also be used for
     * any sort of named value container handled by each symbol level.
     */
    class Symbol* symbol = nullptr;

    /**
     * Non-zero if the query is targeted to a specific track rather than
     * globally, or for the selected track.
     *
     * Note that unlike UIAction, there is no "group" scope for queries.  Queries
     * are either for a global parameter or a track parameter.
     */
    int scope =  0;

    /**
     * Returned value of the symbol.
     */
    int value = 0;

    /**
     * Flag set if the symbol is associated with a valid
     * object, but the value cannot be returned synchronously.
     * todo: for future development.
     */
    bool async = false;
};

