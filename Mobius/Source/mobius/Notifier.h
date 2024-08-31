/**
 * Classes that processes pending nofifications.
 */

#pragma once

class Notifier
{
  public:

    Notifier();
    ~Notifier();
    void setPool(class MobiusPools* p);

    Notification* alloc();
    void add(Notification* n);
    

    void afterEvent(int track);
    void afterTrack(int track);
    void afterBlock();

    void add(class Notificiation* n);
    
  private:

    void flush();

    class MobiusPools* pool = nullptr;
    class Notification* head = nullptr;
    class Notification* tail = nullptr;
    
};

