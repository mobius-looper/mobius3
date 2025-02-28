
#include <JuceHeader.h>

#include "util/Trace.h"
#include "util/Util.h"
#include "model/Session.h"

// only for Setup migration
#include "model/MobiusConfig.h"
#include "model/Setup.h"
#include "model/ParameterSets.h"

#include "Provider.h"
#include "FileManager.h"
#include "ModelTransformer.h"

#include "SessionClerk.h"

SessionClerk::SessionClerk(Provider* p)
{
    provider = p;
}

SessionClerk::~SessionClerk()
{
}

juce::OwnedArray<SessionClerk::Folder>* SessionClerk::getFolders()
{
    return &folders;
}

/**
 * Read the sessions defined in the user library.
 *
 * todo: Need the notion of external sessions that were saved in random locations.
 * Since these aren't very complicated they can go in SystemConfig.
 */
void SessionClerk::initialize()
{
    juce::File root = provider->getRoot();
    bool thisIsFine = true;
    
    juce::File sessions = root.getChildFile("sessions");
    if (sessions.existsAsFile()) {
        // not allowed, something went wrong with installation or they damanged it
        Trace(1, "SessionClerk: Sessions library folder exists as a file");
        thisIsFine = false;
    }
    else if (!sessions.isDirectory()) {
        juce::Result res = sessions.createDirectory();
        if (res.failed()) {
            Trace(1, "SessionClerk: Unable to create sessions library folder");
            Trace(1, "  %s", res.getErrorMessage().toUTF8());
            thisIsFine = false;
        }
    }

    if (thisIsFine) {
        
        libraryRoot = sessions;
        libraryValid = true;
        
        int types = juce::File::TypesOfFileToFind::findDirectories;
        bool recursive = false;
        juce::String pattern = "*";
        juce::Array<juce::File> files = sessions.findChildFiles(types, recursive, pattern,
                                                                juce::File::FollowSymlinks::no);
    
        for (auto file : files) {
            juce::String path = file.getFullPathName();

            Folder* f = new Folder();
            f->name = file.getFileNameWithoutExtension();
            f->path = path;

            // todo: need to look inside and validate contents
            f->valid = true;

            folders.add(f);
        }

        // always ensure that a Default session exists
        bool bootstrapped = bootstrapDefaultSession();
        
        // convert old Setup objects into Sessions
        // normally only done when bootstrapping, but for temporary
        // testing of the Session migration may be done every startup
        migrateSetups(bootstrapped);
    }
}

/**
 * On a fresh install (or a corrupted install) if we don't
 * find the Default session in the library, attempt to create one.a
 * During the period immediately after build 30, this will look for
 * session.xml in the root of the installation directory and copy it to the
 * new folder so we can retain the early settings without corrupting them.
 *
 * Returning true will then trigger the migration of Setups from the
 * MobiusConfig into new Sessions in the library.
 */
bool SessionClerk::bootstrapDefaultSession()
{
    bool bootstrapped = false;
    Folder* f = findFolder(juce::String("Default"));
    if (f == nullptr) {
        juce::File sessionRoot = libraryRoot.getChildFile("Default");
        juce::Result res = sessionRoot.createDirectory();
        if (res.failed()) {
            Trace(1, "SessionClerk: Unable to create defualt session folder");
            Trace(1, "  %s", res.getErrorMessage().toUTF8());
        }
        else {
            juce::File root = provider->getRoot();
            juce::File old = root.getChildFile("session.xml");
            juce::File dest = sessionRoot.getChildFile("session.xml");
            bool copied = false;
            if (old.existsAsFile()) {
                copied = old.copyFileTo(dest);
                if (!copied)
                  Trace(1, "SessionClerk: Unable to convert old session.xml file");
            }

            if (!copied) {
                Session empty;
                dest.replaceWithText(empty.toXml());
            }
                
            f = new Folder();
            f->name = "Default";
            f->path = sessionRoot.getFullPathName();
            f->valid = true;
            
            folders.add(f);
            bootstrapped = true;
        }
    }
    return bootstrapped;
}

//////////////////////////////////////////////////////////////////////
//
// Basic File Management
//
//////////////////////////////////////////////////////////////////////

SessionClerk::Folder* SessionClerk::findFolder(juce::String name)
{
    Folder* found = nullptr;
    for (auto f : folders) {
        if (f->name == name) {
            found = f;
            break;
        }
    }
    return found;
}

juce::XmlElement* SessionClerk::readSessionElement(juce::File src)
{
    juce::XmlElement* result = nullptr;

    juce::String xml = src.loadFileAsString();
    if (xml.length() == 0) {
        Trace(2, "SessionClerk: Empty session file %s", src.getFullPathName().toUTF8());
    }
    else {
        juce::XmlDocument doc(xml);
        std::unique_ptr<juce::XmlElement> docel = doc.getDocumentElement();
        if (docel == nullptr) {
            Trace(1, "SessionClerk: Error parsing %s", src.getFullPathName().toUTF8());
            Trace(1, "  %s", doc.getLastParseError().toUTF8());
        }
        else if (!docel->hasTagName("Session")) {
            Trace(1, "SessionClerk: Incorrect XML element in file %s", src.getFullPathName().toUTF8());
        }
        else {
            // set it free
            result = docel.release();
        }
    }
    return result;
}

void SessionClerk::logErrors(const char* filename, juce::StringArray& errors)
{
    if (errors.size() > 0) {
        Trace(1, "SessionClerk: Errors parsing %s", filename);
        for (auto error : errors)
          Trace(1, "  %s", error.toUTF8());
    }
}

/**
 * Given a folder from the library read the session.xml file and
 * create a Session object.  The Session is owned by the caller
 * and must be cached or deleted.
 */
Session* SessionClerk::readSession(Folder* f, juce::StringArray& errors)
{
    Session* session = nullptr;
    if (f != nullptr) {
        juce::File root (f->path);
        if (!root.isDirectory()) {
            addError(errors, juce::String("Missing session folder"));
            addError(errors, f->path);
        }
        else {
            juce::File src = root.getChildFile("session.xml");
            juce::XmlElement* el = readSessionElement(src);
            if (el != nullptr) {
                session = new Session();
                session->parseXml(el, errors);
                delete el;

                // it doesn't matter what the .xml file had for name
                // it gets the name from the folder it was in
                session->setName(f->name);

                // only do this for bootstrap
                //fixSession(session);

                // this one happens all the time
                if (upgradeSession(session)) {
                    writeSession(f, session, errors);
                }
            }
        }
    }
    return session;
}

/**
 * Short term kludge to fix a few parameter names that should
 * have been different but are now out there.
 *
 * Really only need this for the bootsrap session.
 */
void SessionClerk::fixSession(Session* s)
{
    for (int i = 0 ; i < s->getTrackCount() ; i++) {
        Session::Track* t = s->getTrackByIndex(i);
        ValueSet* values = t->ensureParameters();
        MslValue* v = values->get("group");
        if (v != nullptr) {
            juce::String gname (v->getString());
            values->remove("group");
            values->setJString("trackGroup", gname);
        }

        // for a time the sync parameters were messed up and
        // used the wrong enumeration for storage, 
        v = values->get("syncSource");
        if (v != nullptr) {
            if (StringEqual(v->getString(), "default"))
              v->setString("none");
        }

        v = values->get("trackSyncUnit");
        if (v != nullptr) {
            if (StringEqual(v->getString(), "default"))
              v->setString("loop");
        }
    }
}

/**
 * Once sessions are out there and in use, this is where we make surgical changes
 * to the model.  Doing this incrementally rather than in a big upgrade pass at
 * the start, could go either way but seems less disruptive to do them incrementally.
 *
 * The transformations are this:
 *
 * 1) Session defaultPreset
 *
 * This was formerly Setup::defaultPreset and would be the default starting Preset
 * used by all tracks in this Setup.  If not defined it reversed to the preset named
 * "Default".   If this is set, the parameter set with that name is found and MERGED
 * into the session.  defaultPreset is taken out so we don't do this again.  This is
 * NOT modeled with the sessionOverlay, the session simply becomes a self-contained
 * parameter set with whatever the default was.  This is slightly less flexible because
 * if you then edit that parameter set, Setup/Sessions that used it won't get the new
 * values, you would need to edit them one at a time.  But it gets people out of the habbit
 * of thinking they always need a Preset/ParameterSet to initialize a session.
 *
 * 2) Session::Track trackPreset
 *
 * If this is specified and is the same as defaultPreset it is removed
 * If this is specified and is different than defaultPreset it is retained
 * but renamed to trackOverlay.
 */
bool SessionClerk::upgradeSession(Session* s)
{
    bool modified = false;
    ParameterSets* sets = provider->getParameterSets();
    
    // phase 1: defaultPreset
    ValueSet* globals = s->ensureGlobals();
    const char* defaultName = nullptr;
    MslValue* dpv = globals->get("defaultPreset");
    if (dpv != nullptr) {
        defaultName = dpv->getString();
        ValueSet* defaults = nullptr;
        if (sets != nullptr)
          defaults = sets->find(juce::String(defaultName));
        if (defaults == nullptr)
          Trace(1, "SessionClerk: Invalid defaultPreset %s", dpv->getString());
        else {
            // remove the things that we don't consider to be in the preset any more?
            // the problem child here is loopCount, go ahead and pick that up since
            // if we're here it won't have been manually edited
            globals->assimilate(defaults);
            modified = true;
        }
    }
    else {
        // now the ugly part
        // there isn't a reliable way to test to see if Default has been copied
        // in yet without leaving something behind.  build 33 added parameter forms
        // that would have populated the session with default values
        
        // the convention has been that the first Preset was named "Default" but
        // it was actually just whatever the first one was
        defaultName = "Default";
        
        if (globals->get("upgraded") == nullptr) {
            if (sets != nullptr) {
                ValueSet* defaults = sets->find(defaultName);
                if (defaults != nullptr) {
                    globals->assimilate(defaults);
                }
            }
            globals->setBool("upgraded", true);
            modified = true;
        }
    }
    
    // phase 2: trackPreset
    for (int i = 0 ; i < s->getTrackCount() ; i++) {
        Session::Track* t = s->getTrackByIndex(i);
        ValueSet* trackValues = t->ensureParameters();
        MslValue* v = trackValues->get("trackPreset");
        if (v != nullptr) {
            // only carry this foward if it differs from the defaultPreset
            if (!StringEqual(defaultName, v->getString())) {
                trackValues->setString("trackOverlay", v->getString());
            }
            // once this is converted, remove it so trackOverlay becomes authoritative
            trackValues->remove("trackPreset");
            modified = true;
        }
    }

    // remove this so we don't do it again
    // note that this has to be done last because we need defualtName
    // to remain valid during the track iteration
    if (dpv != nullptr) {
        globals->remove("defaultPreset");
        modified = true;
    }

    return modified;
}

/**
 * Write a modified Session back to the library folder.
 */
void SessionClerk::writeSession(Folder* f, Session* s, juce::StringArray& errors)
{
    if (f != nullptr && s != nullptr) {
        juce::File root (f->path);
        if (!root.isDirectory()) {
            addError(errors, juce::String("Unable to access folder for session ") + f->name);
        }
        else {
            juce::File dest = root.getChildFile("session.xml");

            // this doesn't matter since we fix it on read,
            // but make sure it matches to avoid confusion
            s->setName(f->name);
        
            if (!dest.replaceWithText(s->toXml()))
              addError(errors, juce::String("Failure writing session.xml file for ") + f->name);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Public Interface
//
//////////////////////////////////////////////////////////////////////

/**
 * This one is special, we don't bail if it doesn't exist.
 */
Session* SessionClerk::readDefaultSession(juce::StringArray& errors)
{
    Session* session = nullptr;

    Folder* f = findFolder(juce::String("Default"));
    if (f == nullptr) {
        // bootstratpDefaultSession should have done this during initialize()
        addError(errors, "Default session not found");
    }
    else {
        session = readSession(f, errors);
    }

    // prevent NPEs
    if (session == nullptr) {
        Trace(1, "SessionClerk: Unable to read default session, you will lose");
        session = new Session();
    }
    
    return session;
}

Session* SessionClerk::readSession(juce::String name, juce::StringArray& errors)
{
    Session* session = nullptr;
    
    Folder* f = findFolder(name);
    if (f == nullptr) {
        addError(errors, juce::String("Session ") + name + " not found");
    }
    else {
        session = readSession(f, errors);
    }
    
    return session;
}

void SessionClerk::saveSession(Session* s, juce::StringArray& errors)
{
    juce::String name = s->getName();
    Folder* f = findFolder(name);
    if (f == nullptr) {
        // what would this mean?
        addError(errors, juce::String("Unable to save session ") + name);
        addError(errors, juce::String("No session with that name found"));
    }
    else {
        writeSession(f, s, errors);
        s->setModified(false);
    }
}

///////////////////////////////////////////////////////////////////////
//
// Migration
//
///////////////////////////////////////////////////////////////////////

/**
 * During the Session migration phase, convert the MobiusConfig globals
 * and Setups into Sessions.
 *
 * If the bootstrap flag is on, it means that we did not detect a Default
 * session on startup, an empty one was created, and we must perform a full
 * migration of the Default session.
 *
 * If the bootstrap flag is off, it means we already had a Default session,
 * but we may choose to refresh portions of it from the MobiusConfig for testing.
 * This is temporary.
 *    
 * When bootstrap is on, the Session was created by copying the first prototype
 * session.xml into the session library folder and this is where Midi track definitions
 * for the earlier releases lived.  That session has no audio tracks.
 * When the Default Setup is encounered it needs to merge into the Default
 * Setup rather than creating a new one.
 *
 * NOTE: So that user can downgrade to earlier builds, it is important that
 * we NOT TOUCH either mobius.xml or the original session.xml.
 */
void SessionClerk::migrateSetups(bool bootstrapped)
{
    ModelTransformer transformer(provider);
    
    MobiusConfig* config = provider->getOldMobiusConfig();

    // the default setup is almost always named "Default" but if we don't see one,
    // take the first one
    Setup* defaultSetup = nullptr;
    for (Setup* setup = config->getSetups() ; setup != nullptr ; setup = setup->getNextSetup()) {
        juce::String setupName (setup->getName());
        if (setupName == "Default")
          defaultSetup = setup;
        else if (defaultSetup == nullptr)
          defaultSetup = setup;
    }

    // special one time handling of the bootstrap session
    if (bootstrapped) {
        Folder* f = findFolder("Default");
        if (f == nullptr) {
            // can't happen if bootstrapDefaultSession did it's job
            Trace(1, "SessionClerk: Default session not found during migration");
        }
        else {
            juce::StringArray errors;
            Session* dest = readSession(f, errors);
            if (dest == nullptr) {
                Trace(1, "SessionClerk: Unable to read bootstrap Session %s", f->name.toUTF8());
            }
            else {
                // fix some bad names in the prototype session.xml
                fixSession(dest);
                
                // two problems with track counts
                // 1) MobiusConfig core track count is what was authoritative but there can
                // be more SetupTracks in the object than are actually used
                // 2) Similar issue in Session with midiCount being smaller than the
                // number of TypeMidi tracks

                // The second problem isn't a migration, it's fixing a bad prototype session
                // and we can do that now
                dest->reconcileTrackCount(Session::TypeMidi, dest->getOldMidiTrackCount());

                // copy the globals
                // this is also where MobiusConfig::trackCount is ready and the
                // audio tracks in the session are reconciled
                transformer.addGlobals(config, dest);
                
                if (defaultSetup != nullptr) {
                    // this does a careful merge into the existing session.xml rather
                    // than a full transform so we can preserve Midi track definitions
                    // and flesh out audio track definitions
                    transformer.merge(defaultSetup, dest);
                }
                else {
                    // The MobiusConfig was empty, unusual but could happen
                    // default SessionTracks will have been stubbed out
                    Trace(1, "SessionClerk: No default Setup found during migration");
                }
                writeSession(f, dest, errors);
                delete dest;
            }
        }
    }

    // now migrate all Sessions other than the Default
    
    for (Setup* setup = config->getSetups() ; setup != nullptr ; setup = setup->getNextSetup()) {

        if (setup != defaultSetup) {
            
            juce::String setupName (setup->getName());
            Folder* f = findFolder(setupName);
            if (f == nullptr) {
                if (!bootstrapped) {
                    // this is after the bootstrap period, and we found a new Setup
                    // should only happen if a prior migration failed or they copied
                    // a different mobius.xml file into the installation
                    // not really a problem, but unusual
                    Trace(1, "SessionClerk: Encountered new Setup after initial migration %s",
                          setup->getName());
                }
                Trace(2, "SessionClerk: Migrating Setup %s", setup->getName());

                // SetupTrack counts can be off from what the MobiusConfig said it would be
                // set up globals first to get the right count
                // former global parameters are duplicated in every session
                Session* neu = new Session();
                transformer.addGlobals(config, neu);

                // after getting the track counts right, then migrate the tracks
                transformer.merge(setup, neu);

                juce::StringArray errors;
                (void)createSession(neu, errors);
                delete neu;
            }
            else {
                // we've already seen this one, normally would ignore it but
                // have a merge option just for testing
                bool testMerge = false;
                if (testMerge) {
                    Trace(2, "SessionClerk: Merging Setup %s", setup->getName());
                    juce::StringArray errors;
                    Session* dest = readSession(f, errors);
                    if (dest == nullptr) {
                        Trace(1, "SessionClerk: Unable to read Session to merge into %s", f->name.toUTF8());
                    }
                    else {
                        transformer.merge(setup, dest);
                        writeSession(f, dest, errors);
                        delete dest;
                    }
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Producer and SessionManager
//
//////////////////////////////////////////////////////////////////////

/**
 * Create a new session.
 * An error is returned if a session with this name already exists.
 */
void SessionClerk::createSession(Session* neu, juce::StringArray& errors)
{
    juce::String name = neu->getName();
    if (name.length() == 0) {
        addError(errors, "Missing session name");
    }
    else if (validateFileName(name, errors)) {
        // this can't fail
        juce::String xml = neu->toXml();
        // returns nullptr if it failed and left errors
        Folder* f = createFolder(name, errors);
        if (f != nullptr) {
            juce::File dir (f->path);
            if (!dir.isDirectory()) {
                // shouldn't be here if createFolder said it succeeded
                addError(errors, "Unable to access session library folder");
            }
            else {
                juce::File dest = dir.getChildFile("session.xml");
                if (!dest.replaceWithText(xml))
                    addError(errors, "Session file write failed");
            }

            if (errors.size() > 0)
              delete f;
            else
              folders.add(f);
        }
    }
}

/**
 * Check the name for invalid characters before we pass it off to the
 * Juce file functions since it is unclear how verbose they'll be about
 * what went wrong.
 *
 * Could be a lot more rigorous here but catch the usual punctuation.
 */
bool SessionClerk::validateFileName(juce::String name, juce::StringArray& errors)
{
    bool valid = false;
    if (name.containsAnyOf("\\/$."))
      errors.add("Session name contains illegal punctuation");
    else
      valid = true;
    return valid;
}

SessionClerk::Folder* SessionClerk::createFolder(juce::String name, juce::StringArray& errors)
{
    Folder* result = nullptr;
    
    if (!libraryValid) {
        addError(errors, "Library folder is invalid");
    }
    else {
        Folder* f = findFolder(name);
        if (f != nullptr) {
            addError(errors, juce::String("Session " + name + " already exists"));
        }
        else {
            juce::File dir = libraryRoot.getChildFile(name);
            juce::Result res = dir.createDirectory();
            if (res.failed()) {
                errors.add("Unable to create session folder");
                errors.add(res.getErrorMessage());
            }
            else {
                result = new Folder();
                result->name = name;
                result->path = dir.getFullPathName();
                result->valid = true;
            }
        }
    }
    return result;
}

void SessionClerk::addError(juce::StringArray& errors, juce::String msg)
{
    errors.add(msg);
    juce::String tracemsg = juce::String("SessionClerk: ") + msg;
    Trace(1, "%s", tracemsg.toUTF8());
}

void SessionClerk::deleteSession(juce::String name, juce::StringArray& errors)
{
    Folder* f = findFolder(name);
    if (f == nullptr) {
        addError(errors, juce::String("Session ") + name + " does not exist");
    }
    else {
        juce::File dir (f->path);
        if (!dir.isDirectory()) {
            addError(errors, juce::String("Unable to locate session folder"));
            addError(errors, f->path);
            // it shouldn't have been there to begin with, someone must
            // have deleted it out from under us
            removeFolder(f);
        }
        else {
            // will want to be a lot more selective about how deletion
            // is performed, especially if there is content do one file at a time
            // and fail better
            // this interface doesn't say anything about why it failed
            if (!dir.deleteRecursively()) {
                addError(errors, juce::String("Errors during deletion of session folder"));
                addError(errors, f->path);
            }
            
            // take it out of the folder list even if the delete failed
            // because we can't tell what shape it's in without further
            // forensics
            removeFolder(f);
        }
    }
}

/**
 * Take the Folder object out of the list (assuming it is there)
 * and delete it.
 */
void SessionClerk::removeFolder(Folder* f)
{
    int index = folders.indexOf(f);
    if (index < 0) {
        // odd, where did it come from?
        Trace(1, "SessionClerk: Found zombie folder");
        delete f;
    }
    else {
        folders.remove(index);
    }
}

void SessionClerk::copySession(juce::String name, juce::String newName, juce::StringArray& errors)
{
    Folder* src = findFolder(name);
    Folder* other = findFolder(newName);

    if (!libraryValid) {
        addError(errors, "Library folder is invalid");
    }
    else if (src == nullptr) {
        addError(errors, juce::String("Session ") + name + " does not exist");
    }
    else if (other != nullptr) {
        addError(errors, juce::String("Session ") + name + " already exists");
    }
    else {
        juce::File srcdir (src->path);
        if (!srcdir.isDirectory()) {
            addError(errors, juce::String("Unable to locate session folder"));
            addError(errors, src->path);
            // it shouldn't have been there to begin with, someone must
            // have deleted it out from under us
            removeFolder(src);
        }
        else if (validateFileName(newName, errors)) {
            // returns nullptr if it failed and left errors
            Folder* dest = createFolder(newName, errors);
            if (dest != nullptr) {

                // way too much can go wrong in here, need to break this
                // out and do our own recursive copy once we start having
                // content
                juce::File destdir (dest->path);

                bool status = srcdir.copyDirectoryTo(destdir);

                if (!status) {
                    addError(errors, "Failed to copy session folder to:");
                    addError(errors, dest->path);
                    // at this point, the copy may have done nothing or it may
                    // have failed in the middle, try to clean up
                    if (!destdir.deleteRecursively()) {
                        addError(errors, "Unable to cleanup failed session folder");
                    }
                    delete dest;
                }
                else {
                    folders.add(dest);

                    // technically we should now read the session.xml file
                    // and change the name, but can also just leave it there and
                    // fix it when it is read
                }
            }
        }
    }
}

void SessionClerk::renameSession(juce::String name, juce::String newName, juce::StringArray& errors)
{
    Folder* src = findFolder(name);
    Folder* other = findFolder(newName);

    if (!libraryValid) {
        addError(errors, "Library folder is invalid");
    }
    else if (src == nullptr) {
        addError(errors, juce::String("Session ") + name + " does not exist");
    }
    else if (other != nullptr) {
        addError(errors, juce::String("Session ") + name + " already exists");
    }
    else {
        juce::File srcdir (src->path);
        if (!srcdir.isDirectory()) {
            addError(errors, juce::String("Unable to locate session folder"));
            addError(errors, src->path);
            // it shouldn't have been there to begin with, someone must
            // have deleted it out from under us
            removeFolder(src);
        }
        else if (validateFileName(newName, errors)) {

            // just rename the outer directory
            // technically need to read the session.xml and change the name
            // but we can fix it on load
            
            juce::File destdir = libraryRoot.getChildFile(newName);

            if (!srcdir.moveFileTo(destdir)) {
                addError(errors, "Unable to rename session folder");
            }
            else {
                src->name = newName;
                src->path = destdir.getFullPathName();
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
