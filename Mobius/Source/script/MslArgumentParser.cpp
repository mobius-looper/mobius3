
#include "MslContext.h"
#include "MslValue.h"

MslArgumentParser::MslArgumentParser(MslAction* action)
{
    list = action->arguments;
}

void MslArgumentParser::seek(int index)
{
    if (index < position) {
        item = list;
        position = 0;
    }

    while (item != nullptr && position < index)
      advance();
        
    return item;
}

void MslArgumentParser::advance()
{
    if (item != nullptr) {
        item = item->next;
        position++;
    }
}

MslValue* MslArgumentParser::consume()
{
    MslValue* result = item;
    advance();
    return result;
}

MslValue* MslArgumentParser::get(int index)
{
    seek(index);
    return item;
}

MslArgumentParser::KeywordArg* MslArgumentParser::consumeKeyword()
{
    KeywordArg* result = nullptr;
    
    if (item != nullptr) {
        keywordArg.init();
        result = &keywordArg;
        
        if (item->type != MslValue::Keyword)
          keywordArg.error = true;
        else {
            keywordArg.name = item->getString();
            advance();
            if (item == nullptr)
              keywordArg.error = true;
            else {
                keywordArg.value = item;
                advance();
            }
        }
    }
    return result;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
