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
    struct KeywordArg {
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

    MslValue* get(int index);
    void seek(int index);

    KeywordArg* nextKeywordArg();
    
  private:

    MslValue* list = nullptr;
    MslValue* item = nullptr;
    int position = 0;

    KeywordArg keywordArg;
    
};    
