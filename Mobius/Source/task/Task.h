/**
 * Common base class for system tasks.
 *
 * A task can be used for a number of things, but is essentially a sequence
 * of steps that are performed in an order, with some of those steps requiring
 * asynchronous user interaction.
 *
 * It is a new Mobius component that has overlap with a number of other older things
 * that will eventually be redesigned to become tasks.
 *
 */

#include <JuceHeader.h>

class Task
{
  public:

    Task(class Provider* p);
    virtual ~Task();

    void launch();
    bool isFinished();

    virtual void run() = 0;

  protected:

    class Provider* provider = nullptr;
    bool finished = false;
};
