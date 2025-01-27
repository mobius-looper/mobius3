 
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Session.h"
#include "../../Provider.h"
#include "../JuceUtil.h"

#include "ParameterForm.h"
#include "SessionTrackTable.h"
#include "SessionTreeForms.h"

#include "SessionTrackEditor.h"

SessionTrackEditor::SessionTrackEditor()
{
    tracks.reset(new SessionTrackTable());

    addAndMakeVisible(tracks.get());

    // one of these will become visible on the first load
    addChildComponent(audioForms);
    addChildComponent(midiForms);
    
    tracks->setListener(this);

    currentTrack = 1;
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

    audioForms.initialize(p, juce::String("sessionAudioTrack"));
    midiForms.initialize(p, juce::String("sessionMidiTrack"));
}

void SessionTrackEditor::decacheForms()
{
    audioForms.decache();
    midiForms.decache();
}

void SessionTrackEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    // the one column in the track header is 200 and
    // getting a horizontal scroll bar if the outer track is
    // the same size, give it a little extra
    tracks->setBounds(area.removeFromLeft(204));
    
    audioForms.setBounds(area);
    midiForms.setBounds(area);
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
 * Save is a problem.
 * 
 * SessionGlobalEditor was able to keep all editing state in the Forms
 * which could then just be dumped into the destination Session.
 *
 * TrackEditor is more complex because it has been dumping intermediate state
 * into the initial loaded Session as it transitions between tracks.  So some
 * of the editing state is in the initial Session and some is in the Forms.
 *
 * So we have a data merge problem.  You can't just replace the ValueSets in the
 * destination session with the ValueSets in the edited session because not everything
 * in the Session was displayed in the forms and some parameters may have changed
 * outside the SessionEditor, such as in the MixerEditor or something new in the future.
 *
 * The only things we are allowed to touch are the things managed by the forms in use.
 * So the transfer is somewhat awkward:
 *
 *     for each track
 *        load all forms from the initial session
 *        save all forms into the destination session
 *
 * The Track forms effectively perform the filtered merge.
 */
void SessionTrackEditor::save(Session* dest)
{
    // save editing state for the current track back to the intermediate session
    saveForms(currentTrack);

    // make the numbers match
    dest->reconcileTrackCount(Session::TypeAudio, session->getAudioTracks());
    dest->reconcileTrackCount(Session::TypeMidi, session->getMidiTracks());

    // now load the intermediate session back into the forms one at a time
    // but save them to the destination session
    for (int i = 0 ; i < session->getTrackCount() ; i++) {
        
        Session::Track* srcTrack = session->getTrackByIndex(i);
        Session::Track* destTrack = dest->getTrackByNumber(srcTrack->number);
        if (destTrack == nullptr) {
            // should not be seeing this if reconcileTrackCount worked to flesh them out
            Trace(1, "SessionTrackEditor: New track save failed");
        }
        else {
            // this is the only thing that isn't in the ValueSet
            destTrack->name = srcTrack->name;
            
            ValueSet* srcValues = srcTrack->ensureParameters();
            ValueSet* destValues = destTrack->ensureParameters();

            if (srcTrack->type == Session::TypeMidi) {
                midiForms.load(srcValues);
                midiForms.save(destValues);
            }
            else {
                audioForms.load(srcValues);
                audioForms.save(destValues);
            }
        }
    }
}

/**
 * Have to propagate a cancel down to clear out lingering references
 * to a Session's ValueTrees.
 */
void SessionTrackEditor::cancel()
{
    audioForms.cancel();
    midiForms.cancel();
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
    
    int newNumber = tracks->getSelectedTrackNumber();
    if (newNumber == 0) {
        Trace(1, "SessionTrackEditor: Change alert with no selected track number");
    }
    else if (newNumber != currentTrack) {
        
        saveForms(currentTrack);
        
        loadForms(newNumber);
        
        currentTrack = newNumber;
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
        saveForms(currentTrack);

        int adjustedRow = desiredRow;
        if (desiredRow > sourceRow)
          desiredRow--;

        // this does the structural changes in the Session
        session->move(sourceRow, desiredRow);

        tracks->reload();

        // keep on the same object
        currentTrack = desiredRow;
        // should already be there but make sure it's in sync
        tracks->selectRow(desiredRow);
        loadForms(currentTrack);
    }
}

/**
 * TrackTable would like to add a new track of the given type
 */
void SessionTrackEditor::addTrack(Session::TrackType type)
{
    int currentTypeCount;
    if (type == Session::TypeAudio)
      currentTypeCount = session->getAudioTracks();
    else
      currentTypeCount = session->getMidiTracks();

    saveForms(currentTrack);
      
    session->reconcileTrackCount(type, currentTypeCount + 1);

    // it is ecpected to have added it at the end
    currentTrack = session->getTrackCount() - 1;

    tracks->reload();
    tracks->selectRow(currentTrack);
    
    loadForms(currentTrack);
}

void SessionTrackEditor::deleteTrack(int number)
{
    saveForms(currentTrack);

    session->deleteByNumber(number);

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
 */
void SessionTrackEditor::loadForms(int number)
{
    Session::Track* trackdef = session->getTrackByNumber(number);
    if (trackdef == nullptr) {
        Trace(1, "SessionTrackEditor: Invalid track number %d", number);
    }
    else {
        ValueSet* values = trackdef->ensureParameters();
        if (trackdef->type == Session::TypeMidi) {
            midiForms.load(values);
            audioForms.setVisible(false);
            midiForms.setVisible(true);
        }
        else {
            audioForms.load(values);
            midiForms.setVisible(false);
            audioForms.setVisible(true);
        }
    }
}

void SessionTrackEditor::saveForms(int number)
{    
    Session::Track* trackdef = session->getTrackByNumber(number);
    if (trackdef == nullptr) {
        Trace(1, "SessionTrackEditor: Invalid intermediate session track number %d", number);
    }
    else {
        ValueSet* values = trackdef->ensureParameters();
        if (trackdef->type == Session::TypeMidi)
          midiForms.save(values);
        else
          audioForms.save(values);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

        
    
    

