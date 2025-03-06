
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Session.h"
#include "../../model/SessionConstants.h"
#include "../../Provider.h"
#include "../JuceUtil.h"

#include "../parameter/ParameterForm.h"
#include "SessionTrackTable.h"
#include "SessionTreeForms.h"

#include "SessionTrackEditor.h"

SessionTrackEditor::SessionTrackEditor()
{
    tracks.reset(new SessionTrackTable());

    addAndMakeVisible(tracks.get());

    tracks->setListener(this);

    currentTrack = 0;
}

SessionTrackEditor::~SessionTrackEditor()
{
}

void SessionTrackEditor::initialize(Provider* p)
{
    // temporarily save this until SessionTrackTable can
    // get everything it needs from just the hsession
    provider = p;
    
    tracks->initialize(p, this);
}

void SessionTrackEditor::decacheForms()
{
    for (auto form : trackForms)
      form->decache();
}

void SessionTrackEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    // the one column in the track header is 200 and
    // getting a horizontal scroll bar if the outer track is
    // the same size, give it a little extra
    tracks->setBounds(area.removeFromLeft(204));
    
    for (auto form : trackForms)
      form->setBounds(area);
}

void SessionTrackEditor::load(Session* s)
{
    // we are allowed to retain a pointer to this so track forms
    // can pull in new values as selections in the table and trees change
    session = s;
    
    // track configuration may change
    tracks->load(provider, s);
    
    // forms show the selected track
    loadForms(currentTrack);
}

/**
 * Save is complicated.
 *
 * If forms were displayed for a track, they need to be saved to our
 * edited Session copy.
 *
 * After that we need to replace Session::Tracks in the destination session
 * with ours, adding new ones, and removing ones that were deleted.
 */

void SessionTrackEditor::save(Session* dest)
{
    // get everything out of the forms and back into the Session copy
    for (auto form : trackForms)
      form->save();

    // bring out the source and destination tracks
    juce::Array<Session::Track*> editedTracks;
    session->steal(editedTracks);

    juce::Array<Session::Track*> originalTracks;
    dest->steal(originalTracks);

    juce::Array<Session::Track*> resultTracks;

    // could be smarter and avoid some memory churn by only replacing
    // Session::Tracks that were in fact edited, which we can tell if there
    // was a SessionTrackForms object instantiated for it
    
    int srcIndex = 0;
    while (srcIndex < editedTracks.size()) {
        
        Session::Track* srcTrack = editedTracks[srcIndex];
        Session::Track* destTrack = findMatchingTrack(originalTracks, srcTrack->id);
        
        if (destTrack == nullptr) {
            // this must be a new track that wasn't in the original, move it
            // to the result
            editedTracks.removeAllInstancesOf(srcTrack);
            resultTracks.add(srcTrack);
        }
        else {
            // this is the only thing that isn't in the ValueSet
            destTrack->name = srcTrack->name;

            ValueSet* srcValues = srcTrack->stealParameters();
            destTrack->replaceParameters(srcValues);

            // move the merged destTrack from the original list onto the result
            originalTracks.removeAllInstancesOf(destTrack);
            resultTracks.add(destTrack);
            srcIndex++;
        }
    }

    // at this point
    // editedTracks has things that were merged into something from originalTrack
    // originalTracks has tracks left over that were deleted in the new session
    // resultTracks has a combination of new tracks and merged tracks

    deleteRemaining(editedTracks);
    deleteRemaining(originalTracks);
    dest->replace(resultTracks);
}

Session::Track* SessionTrackEditor::findMatchingTrack(juce::Array<Session::Track*>& array, int id)
{
    Session::Track* found = nullptr;
    for (auto t : array) {
        if (t->id == id) {
            found = t;
            break;
        }
    }
    return found;
}

void SessionTrackEditor::deleteRemaining(juce::Array<Session::Track*>& array)
{
    while (array.size() > 0) {
        Session::Track* t = array.removeAndReturn(0);
        delete t;
    }
}

/**
 * Have to propagate a cancel down to clear out lingering references
 * to a Session's ValueTrees.
 */
void SessionTrackEditor::cancel()
{
    for (auto form : trackForms)
      form->cancel();
}

//////////////////////////////////////////////////////////////////////
//
// SessionTrackTable Commands
//
//////////////////////////////////////////////////////////////////////

/**
 * This is called when the selected row changes either by clicking on
 * it or using the keyboard arrow keys after a row has been selected.
 *
 * Save the current forms back to the intermediate session,
 * then display the appropriate tree forms for the new track type
 * and load those forms.
 */
void SessionTrackEditor::typicalTableChanged(TypicalTable* t, int row)
{
    (void)t;
    (void)row;
    
    int newRow = tracks->getSelectedRow();
    if (newRow < 0) {
        Trace(1, "SessionTrackEditor: Change alert with no selected track number");
    }
    else if (newRow != currentTrack) {
        loadForms(newRow);
        currentTrack = newRow;
    }
}

/**
 * The track table would like to move a row.
 * sourceRow is the track INDEX it wants to move and
 * desiredRow is the index the track should have.
 *
 * After the Session tracks are restructured the track table
 * will repaint itself to pull the new model.
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
void SessionTrackEditor::move(int sourceRow, int desiredRow)
{
    if (sourceRow != desiredRow) {

        int adjustedRow = desiredRow;
        if (desiredRow > sourceRow)
          adjustedRow--;

        // this does the structural changes in the Session
        // necessary because the table builds itself from the Session rather
        // than the other way around
        session->move(sourceRow, adjustedRow);

        tracks->reload();

        // keep on the same object
        // should already be there but make sure it's in sync
        tracks->selectRow(desiredRow);
        currentTrack = desiredRow;
        loadForms(currentTrack);
    }
}

/**
 * TrackTable would like to add a new track of the given type
 */
void SessionTrackEditor::addTrack(Session::TrackType type)
{
    saveForms(currentTrack);

    Session::Track* neu = new Session::Track();
    neu->type = type;
    session->add(neu);

    currentTrack = session->getTrackCount() - 1;
    tracks->reload();
    tracks->selectRow(currentTrack);
    
    loadForms(currentTrack);
}

void SessionTrackEditor::deleteTrack(int row)
{
    saveForms(currentTrack);

    session->deleteTrack(row);

    // go back to the beginning, though could try to be one after the
    // deleted one
    currentTrack = 0;

    tracks->reload();
    tracks->selectRow(currentTrack);
    loadForms(currentTrack);
}

void SessionTrackEditor::bulkReconcile(int audioCount, int midiCount)
{
    saveForms(currentTrack);

    session->reconcileTrackCount(Session::TypeAudio, audioCount);
    session->reconcileTrackCount(Session::TypeMidi, midiCount);

    // pick one of the new ones or go back to the top
    currentTrack = 0;

    tracks->reload();
    tracks->selectRow(currentTrack);
    loadForms(currentTrack);
}

//////////////////////////////////////////////////////////////////////
//
// Internal State Transfer
//
//////////////////////////////////////////////////////////////////////

/**
 * Load the current set of editing forms with data from
 * the selected track.
 *
 * To avoid an ugly correspondence, I'm just saving the forms directly
 * in the table rows so we can add/remove/move/ them without having
 * to maintain order in a parallel array or do id searches.
 */
void SessionTrackEditor::loadForms(int row)
{
    SessionTrackForms* forms = nullptr;
    SessionTrackTableRow* row = tracks.getRow(row);
    if (row != nullptr) {
        forms = row->forms;
        if (forms == nullptr) {
            // first time here
            Session::Track* trackdef = session->getTrackByIndex(row);
            if (trackdef == nullptr) {
                Trace(1, "SessionTrackEditor: Invalid track index %d", row);
            }
            else {
                ValueSet* values = trackdef->ensureParameters();

                // demote the Session::Track::name into the ValueSet so it
                // can be dealt with through the ParamTrackName parameter like
                // any of the others
                values->setJString(SessionTrackName, trackdef->name);

                forms = new SessionTrackForms();
                forms->initialize(provider, trackdef);
                trackForms.add(forms);
                addChildComponent(forms);
            }
        }
    }

    if (forms != nullptr) {
        for (auto f : trackForms) {
            if (f == forms)
              f->setVisible(true);
            else
              f->setVisible(false);
        }
    }
}

void SessionTrackEditor::saveForms(int row)
{    
    Session::Track* trackdef = session->getTrackByIndex(row);
    if (trackdef == nullptr) {
        Trace(1, "SessionTrackEditor: Invalid intermediate session track index %d", row);
    }
    else {
        ValueSet* values = trackdef->ensureParameters();
        if (trackdef->type == Session::TypeMidi)
          midiForms.save(values);
        else
          audioForms.save(values);

        // upgrade the track name from the value map back to the track object
        MslValue* v = values->get(SessionTrackName);
        if (v != nullptr) {
            juce::String newName = juce::String(v->getString());
            if (newName != trackdef->name) {
                trackdef->name = newName;
                // refresh the table to have the new name
                tracks->reload();
            }
            values->remove(SessionTrackName);
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

        
    
    

