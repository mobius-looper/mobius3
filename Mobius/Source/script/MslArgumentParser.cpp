
#include "MslContext.h"
#include "MslValue.h"
#include "MslArgumentParser.h"

MslArgumentParser::MslArgumentParser(MslAction* action)
{
    list = action->arguments;
    item = list;
    position = 0;
}

MslValue* MslArgumentParser::seek(int index)
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

bool MslArgumentParser::hasNext()
{
    return (item != nullptr);
}

MslValue* MslArgumentParser::next()
{
    MslValue* result = item;
    advance();
    return result;
}

const char* MslArgumentParser::nextString()
{
    const char* result = nullptr;
    MslValue* v = next();
    if (v != nullptr)
      result = v->getString();
    return result;
}

int MslArgumentParser::nextInt()
{
    // todo: can't return "null" here to mean missing, need something
    // without calling hasNext?
    int result = 0;
    MslValue* v = next();
    if (v != nullptr)
      result = v->getInt();
    return result;
}

MslArgumentParser::Keyarg* MslArgumentParser::nextKeyarg()
{
    Keyarg* result = nullptr;
    
    if (item != nullptr) {
        keyarg.init();
        result = &keyarg;
        
        if (item->type != MslValue::Keyword)
          keyarg.error = true;
        else {
            keyarg.name = item->getString();
            advance();
            if (item == nullptr)
              keyarg.error = true;
            else {
                keyarg.value = item;
                advance();
            }
        }
    }
    return result;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
