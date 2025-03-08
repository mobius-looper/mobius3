
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Session.h"
//#include "../../model/SessionConstants.h"
#include "../../Provider.h"

#include "SessionTrackTable.h"
#include "SessionTrackForms.h"

#include "SessionTrackEditor.h"

//////////////////////////////////////////////////////////////////////
//
// TrackState
//
//////////////////////////////////////////////////////////////////////

SessionTrackEditor::TrackState::TrackState(Session::Track* t)
{
    trackdef.reset(t);
}

Session::Track* SessionTrackEditor::TrackState::getTrack()
{
    return trackdef.get();
}

Session::Track* SessionTrackEditor::TrackState::stealTrack()
{
    return trackdef.release();
}

SessionTrackForms* SessionTrackEditor::TrackState::getForms()
{
    return forms.get();
}

void SessionTrackEditor::TrackState::setForms(SessionTrackForms* f)
{
    if (forms != nullptr)
      Trace(1, "SessionTrackEditor: Attempt to set different track forms");
    else
      forms.reset(f);
}

//////////////////////////////////////////////////////////////////////
//
// SessionTrackEditor
//
//////////////////////////////////////////////////////////////////////

SessionTrackEditor::SessionTrackEditor()
{
}

SessionTrackEditor::~SessionTrackEditor()
{
}

void SessionTrackEditor::initialize(Provider* p, SessionEditor* se)
{
    provider = p;
    editor = se;
    
    table.reset(new SessionTrackTable());
    addAndMakeVisible(table.get());

    table->initialize(this);
    table->setListener(this);

    currentTrack = 0;
}

void SessionTrackEditor::decacheForms()
{
    for (auto state : states) {
        SessionTrackForms* forms = state->getForms();
        if (forms != nullptr)
          forms->decacheForms();
    }
}

void SessionTrackEditor::sessionOverlayChanged()
{
    for (auto state : states) {
        SessionTrackForms* forms = state->getForms();
        if (forms != nullptr)
          forms->sessionOverlayChanged();
    }
}

void SessionTrackEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    // the one column in the track header is 200 and
    // getting a horizontal scroll bar if the outer track is
    // the same size, give it a little extra
    table->setBounds(area.removeFromLeft(204));
    
    for (auto state : states) {
        SessionTrackForms* forms = state->getForms();
        if (forms != nullptr)
          forms->setBounds(area);
    }
}

void SessionTrackEditor::load(Session* s)
{
    session = s;
    states.clear();

    // ownership of the Session::Tracks transfers to the TrackStates
    juce::Array<Session::Track*> defs;
    s->steal(defs);
    for (auto def : defs) {
        // convert the track name field into an entry in the ValueSet which is all
        // forms can deal with
        ValueSet* values = def->ensureParameters();
        values->setString("trackName", def->name.toUTF8());
        
        TrackState* state = new TrackState(def);
        states.add(state);
    }
    table->load(states);
    table->selectRow(0);
    
    // forms show the selected track
    show(currentTrack);
}

void SessionTrackEditor::reload()
{
    for (auto state : states) {
        SessionTrackForms* stf = state->getForms();
        if (stf != nullptr)
          stf->reload();
    }
}

/**
 * The editing copy of the Session had all of the Session::Tracks removed
 * and transferred to the TrackState list.  Some of those may have been
 * deleted or new ones created.  The resulting list replaces the
 * track list in the destination session.
 */
void SessionTrackEditor::save(Session* dest)
{
    // get everything out of the forms and back into the Session::Track
    for (auto state : states) {
        SessionTrackForms* forms = state->getForms();
        if (forms != nullptr)
          forms->save();
    }

    // bring out the new Session::Track list
    juce::Array<Session::Track*> editedTracks;
    for (auto state : states) {
        Session::Track* track = state->stealTrack();
        // uncoonvert the name from within the ValueSet back to a top-level field
        // ugh, these aren't reliably connected to a Session/SymbolTable at this point
        // so have to use names
        MslValue* v = track->get("trackName");
        if (v != nullptr) {
            track->name = juce::String(v->getString());
            track->remove("trackName");
        }
        editedTracks.add(track);
    }

    dest->replace(editedTracks);
}

/**
 * Have to propagate a cancel down to clear out lingering references
 * to a Session's ValueTrees.
 */
void SessionTrackEditor::cancel()
{
    session = nullptr;
    for (auto state : states) {
        SessionTrackForms* forms = state->getForms();
        if (forms != nullptr)
          forms->cancel();
    }
}

//////////////////////////////////////////////////////////////////////
//
// SessionTrackTable Commands
//
//////////////////////////////////////////////////////////////////////

/**
 * This is called when the selected row changes either by clicking on
 * it or using the keyboard arrow keys after a row has been selected.
 */
void SessionTrackEditor::typicalTableChanged(TypicalTable* t, int row)
{
    (void)t;
    (void)row;
    // yuno trust the passed row?
    int newRow = table->getSelectedRow();
    if (newRow < 0) {
        Trace(1, "SessionTrackEditor: Change alert with no selected track number");
    }
    else if (newRow != currentTrack) {
        show(newRow);
        currentTrack = newRow;
    }
}

/**
 * The track table would like to move a row.
 * sourceRow is the track INDEX it wants to move and
 * desiredRow is the index the track should have.
 *
 * oddment:  The sourceRow is the selected row or the row you are ON
 * and want to move, and desiredRow is the row you were over when the
 * mouse was released and where you want it to BE.  The way it seems to work
 * is in two phases, first remove row 0 which shifts everything up.  Then insert
 * the removed row back into the list.  Because of this upward shift, the insertion
 * index needs to be one less than what the drop target was.  Or something like that,
 * maybe I'm just not mathing this right.   Anyway, if you're moving up the
 * two indexes work, but if you're moving down you have to -1 the desiredRow.
 */
void SessionTrackEditor::moveTrack(int sourceRow, int desiredRow)
{
    if (sourceRow != desiredRow) {

        int adjustedRow = desiredRow;
        if (desiredRow > sourceRow)
          adjustedRow--;

        // formerly relied on the Session to do this
        //session->move(sourceRow, adjustedRow);

        if (sourceRow != adjustedRow &&
            sourceRow >= 0 && sourceRow < states.size() &&
            adjustedRow >= 0 && adjustedRow < states.size()) {

            states.move(sourceRow, adjustedRow);
        }
        
        table->load(states);
        // keep on the same object
        // should already be there but make sure it's in sync
        table->selectRow(desiredRow);
        currentTrack = desiredRow;
        show(currentTrack);
    }
}

SessionTrackEditor::TrackState* SessionTrackEditor::getState(int index)
{
    SessionTrackEditor::TrackState* state = nullptr;
    if (index >= 0 && index < states.size())
      state = states[index];
    else
      Trace(1, "SessionTrackEditor: Track index out of range %d", index);
    return state;
}

void SessionTrackEditor::renameTrack(int index, juce::String newName)
{
    TrackState* state = getState(index);
    if (state != nullptr) {
        // while the track is being edited, the name lives inside the ValueSet
        Session::Track* track = state->getTrack();
        ValueSet* values = track->ensureParameters();
        values->setString("trackName", newName.toUTF8());
        // this is the only thing that can edit a track parameter outside the form
        // the one field containing this must be reloaded, but there isn't an easy
        // way to get to that from here so reload all of them, rename from the table is unusual
        show(currentTrack);
    }
}

void SessionTrackEditor::addTrack(Session::TrackType type)
{
    addState(type);
    
    currentTrack = states.size() - 1;
    table->load(states);
    table->selectRow(currentTrack);
    
    show(currentTrack);
}

void SessionTrackEditor::deleteTrack(int row)
{
    deleteState(row);
     
    // go back to the beginning, though could try to be one after the
    // deleted one
    currentTrack = 0;

    table->load(states);
    table->selectRow(currentTrack);
    show(currentTrack);
}

void SessionTrackEditor::bulkReconcile(int audioCount, int midiCount)
{
    reconcileTrackCount(Session::TypeAudio, audioCount);
    reconcileTrackCount(Session::TypeMidi, midiCount);

    // pick one of the new ones or go back to the top
    currentTrack = 0;

    table->load(states);
    table->selectRow(currentTrack);
    show(currentTrack);
}

/**
 * Pulled over the same algorithm from Session::reconcileTrackCount
 * uses because I'm lazy.  Could do both types in one pass if you tried.
 */
void SessionTrackEditor::reconcileTrackCount(Session::TrackType type, int required)
{
    // how many are there now?
    int currentCount = 0;
    for (auto state : states) {
        Session::Track* t = state->getTrack();
        if (t->type == type) {
            currentCount++;
        }
    }

    if (currentCount < required) {
        // add new ones
        while (currentCount < required) {
            addState(type);
            currentCount++;
        }
    }
    else if (currentCount > required) {
        // awkward since they can be in random order
        // seek up to the position after the last track of this type
        int position = 0;
        int found = 0;
        while (position < states.size() && found < required) {
            TrackState* state = states[position];
            Session::Track* t = state->getTrack();
            if (t->type == type)
              found++;
            position++;
        }
        // now delete the remainder
        while (position < states.size()) {
            TrackState* state = states[position];
            Session::Track* t = state->getTrack();
            if (t->type == type)
              deleteState(position);
            else
              position++;
        }
    }
}

void SessionTrackEditor::addState(Session::TrackType type)
{
    Session::Track* neu = new Session::Track();
    neu->type = type;
    
    TrackState* state = new TrackState(neu);
    states.add(state);
}

void SessionTrackEditor::deleteState(int index)
{
    TrackState* state = getState(index);
    if (state != nullptr) {
        SessionTrackForms* forms = state->getForms();
        if (forms != nullptr) {
            // it is important that we take this out of the component hierarchy
            // since they're all about to be deleted
            removeChildComponent(forms);
        }

        // todo: this will lose whatever was configured in this track
        // could save this on an undo list and bring it back if they add a track
        // of this type again
        states.remove(index, true);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Tree/Form Display
//
//////////////////////////////////////////////////////////////////////

/**
 * Show the tree forms for the desired track.
 */
void SessionTrackEditor::show(int row)
{
    TrackState* state = getState(row);
    if (state != nullptr) {
        SessionTrackForms* forms = state->getForms();
        if (forms == nullptr) {
            // first time here
            forms = new SessionTrackForms();
            forms->initialize(provider, editor, session, state->getTrack());
            state->setForms(forms);
            addChildComponent(forms);
            // this will need the size of the others
            resized();
        }

        for (auto s : states) {
            SessionTrackForms* stf = s->getForms();
            if (stf != nullptr) {
                if (stf == forms)
                  stf->setVisible(true);
                else
                  stf->setVisible(false);
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

        
    
    

