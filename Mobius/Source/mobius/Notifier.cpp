
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "Notification.h"
#include "MobiusPools.h"
#include "Notifier.h"

Notifier::Notifier()
{
}

Notifier::~Notifier()
{
    flush();
}

void Notifier::setPool(class MobiusPools* p)
{
    pool = p;
}

Notification* Notifier::alloc()
{
    return pool->newNotification();
}

void Notifier::add(Notification* n)
{
    if (head == nullptr) {
        head = n;
        if (tail != nullptr)
          Trace(1, "Notifier: Lingering tail");
    }
    else if (tail != nullptr) {
        tail->next = n;
    }
    else {
        Trace(1, "Notifier: Missing tail");
        tail = head;
        while (tail->next != nullptr) tail = tail->next;
    }
    tail = n;
}

void Notifier::flush()
{
    while (head != nullptr) {
        Notification* next = head->next;
        head->next = nullptr;
        pool->checkin(head);
        head = next;
    }
    tail = nullptr;
}

/**
 * Process any notifications allowed to happen at the end of an audio block.
 * This also flushes all queued notifications after processing.
 */
void Notifier::afterBlock()
{
    flush();
}

void Notifier::afterEvent(int track)
{
    (void)track;
}

void Notifier::afterTrack(int track)
{
    (void)track;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
