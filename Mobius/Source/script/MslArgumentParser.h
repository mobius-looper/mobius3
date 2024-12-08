/**
 * Utillity class to help the containing application deconstruct the
 * argument list of an MslAction.
 *
 * It's really more of an MslValue list utility, not specific to MslAction.
 * Might want something similar for MslResult?
 */

#pragma once

class MslArgumentParser
{
  public:

    // helper structure to return key/value pairs
    struct Keyarg {
        const char* name = nullptr;
        MslValue* value = nullptr;
        bool error = false;
        void init() {
            name = nullptr;
            value = nullptr;
            error = false;
        }
    };

    MslArgumentParser(class MslAction* action);
    ~MslArgumentParser() {}

    MslValue* seek(int index);
    void advance();
    bool hasNext();
    MslValue* next();
    const char* nextString();
    int nextInt();
    Keyarg* nextKeyarg();
    
  private:

    MslValue* list = nullptr;
    MslValue* item = nullptr;
    int position = 0;

    Keyarg keyarg;
    
};    
