
#include "../../util/Util.h"
#include "Structure.h"

Structure::Structure()
{
    ordinal = 0;
    mNext = nullptr;
    mName = nullptr;
}

/**
 * We have for a long time assumed that
 * deleting one deletes the entire collection.
 * Continue that till we move to proper collections.
 */
Structure::~Structure()
{
	delete mName;

	Structure* el = nullptr;
    Structure* next = nullptr;

	for (el = mNext ; el != nullptr ; el = next) {
		next = el->getNext();
		el->setNext(nullptr);
		delete el;
	}
}

Structure* Structure::getNext()
{
    return mNext;
}

void Structure::setNext(Structure* next)
{
    mNext = next;
}

const char* Structure::getName()
{
	return mName;
}

void Structure::setName(const char* s)
{
	delete mName;
	mName = CopyString(s);
}

/**
 * Assign ordinals to all the elements of a structure list
 * Assumes you are starting from the head of the list.
 */
void Structure::ordinate(Structure* list)
{
    int ord = 0;
    while (list != nullptr) {
        list->ordinal = ord;
        ord++;
        list = list->getNext();
    }
}

int Structure::count(Structure* list)
{
    int count = 0;
    while (list != nullptr) {
        count++;
        list = list->getNext();
    }
    return count;
}

Structure* Structure::find(Structure* list, const char* name)
{
    Structure* found = nullptr;

    while (list != nullptr) {
        if (StringEqual(list->getName(), name)) {
            found = list;
            break;
        }
        list = list->getNext();
    }
    return found;
}

Structure* Structure::append(Structure* list, Structure* neu)
{
	Structure* last = list;
    while (last != nullptr && last->getNext() != nullptr)
      last = last->getNext();

    if (last != nullptr)
      last->setNext(neu);
    else
      list = neu;

    return list;
}

int Structure::getOrdinal(Structure* list, const char* name)
{
    int ordinal = -1;

    // safe to repeast this every time?
    ordinate(list);
    Structure* s = find(list, name);
    if (s != nullptr)
      ordinal = s->ordinal;
    
    return ordinal;
}

Structure* Structure::get(Structure* list, int ordinal)
{
    Structure* found = nullptr;

    ordinate(list);
    while (list != nullptr) {
        if (ordinal == list->ordinal) {
            found = list;
            break;
        }
        list = list->getNext();
    }
    return found;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
