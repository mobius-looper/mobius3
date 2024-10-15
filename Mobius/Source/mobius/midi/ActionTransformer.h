/**
 * The first layer of action processing.
 *
 * Take an action submitted by the user or script, analyze it and apply
 * various transformations.  Send the transformed actions to the Scheduler
 * which will ultimately pass compiled action behavior to the Track.
 */

#pragma once

class ActionTransformer
{
  public:

    ActionTransformer(class MidiTrack* t, class TrackScheduler* s);
    ~ActionTransformer();

    void initialize(class UIActionPool* ap, class SymbolTable* st);

    void doKernelActions(class UIAction* actions);
    void doSchedulerActions(class UIAction* actions);

  private:

    class MidiTrack* track = nullptr;
    class TrackScheduler* scheduler = nullptr;
    class UIActionPool* actionPool = nullptr;
    class SymbolTable* symbols = nullptr;

    void doActions(class UIAction* list, bool owned);
    void doOneAction(class UIAction* a);
    
};
