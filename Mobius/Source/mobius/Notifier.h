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

    class Notification* alloc();
    void add(class Notification* n);
    

    void afterEvent(int track);
    void afterTrack(int track);
    void afterBlock();

  private:

    void flush();

    class MobiusPools* pool = nullptr;
    class Notification* head = nullptr;
    class Notification* tail = nullptr;
    
};

