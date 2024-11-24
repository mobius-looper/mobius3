/**
 * This is the base class for track type specific action handling.
 * BaseScheduler will have an instance of this and pass along actions
 * and events.
 *
 * The primarily implementation si LooperScheduler which handles most of the
 * gory details involved with mode transitions and instructs the AbstractTrack
 * to do things at the right time.
 *
 * Other track types may have far less complex scheduling requirements.
 */

class TrackActionScheduler
{
  public:

    virtual ~TrackActionScheduler() {}

    virtual void doAction(class UIAction* a) = 0;
    virtual void advance(class MoibusAudioStream* stream) = 0;
    virtual void doEvent(class TrackEvent* e) = 0;

};

