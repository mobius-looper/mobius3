
#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/Session.h"

// only for Setup migration
#include "model/MobiusConfig.h"
#include "model/Setup.h"

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
Session* SessionClerk::readSession(Folder* f)
{
    Session* session = nullptr;
    if (f != nullptr) {
        juce::File root (f->path);
        if (!root.isDirectory()) {
            Trace(1, "SessionClerk: Missing session folder %s", f->path.toUTF8());
        }
        else {
            juce::File src = root.getChildFile("session.xml");
            juce::XmlElement* el = readSessionElement(src);
            if (el != nullptr) {
                juce::StringArray errors;
                session = new Session();
                session->parseXml(el, errors);
                logErrors("session.xml", errors);
                delete el;

                // it doesn't matter what the .xml file had for name
                // it gets the name from the folder it was in
                session->setName(f->name);

                fixSession(session);
            }
        }
    }
    return session;
}

/**
 * Short term kludge to fix a few parameter names that should
 * have been different but are now out there.
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
    }
}

/**
 * Write a modified Session back to the library folder.
 */
void SessionClerk::writeSession(Folder* f, Session* s)
{
    if (f != nullptr && s != nullptr) {
        juce::File root (f->path);
        juce::File dest = root.getChildFile("session.xml");

        // this doesn't matter since we fix it on read,
        // but make sure it matches to avoid confusion
        s->setName(f->name);
        
        dest.replaceWithText(s->toXml());
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
Session* SessionClerk::readDefaultSession()
{
    Session* session = nullptr;

    if (libraryValid) {
        Folder* f = findFolder(juce::String("Default"));
        if (f == nullptr) {
            // bootstratpDefaultSession should have done this during initialize()
            Trace(1, "SessionClerk: Default session not found");
        }
        else {
            session = readSession(f);
        }
    }

    // prevent NPEs
    if (session == nullptr) {
        Trace(1, "SessionClerk: Unable to read default session, you will lose");
        session = new Session();
    }
    
    return session;
}

Session* SessionClerk::readSession(juce::String name)
{
    Session* session = nullptr;
    
    Folder* f = findFolder(name);
    if (f == nullptr) {
        Trace(1, "SessionClerk: Unknown session %s", name.toUTF8());
    }
    else {
        session = readSession(f);
    }
    
    return session;
}

void SessionClerk::saveSession(Session* s)
{
    juce::String name = s->getName();
    Folder* f = findFolder(name);
    if (f == nullptr) {
        // what would this mean?
        Trace(1, "SessionClerk: Unable to save session %s", name.toUTF8());
        Trace(1, "SessionClerk: No session with that name found");
    }
    else {
        writeSession(f, s);
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
    
    MobiusConfig* config = provider->getMobiusConfig();

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
            Session* dest = readSession(f);
            if (dest == nullptr) {
                Trace(1, "SessionClerk: Unable to read bootstrap Session %s", f->name.toUTF8());
            }
            else {
                // copy the globals
                transformer.addGlobals(config, neu);
                
                // this is the only time that the MobiusConfig track count is used
                int oldTrackCount = config->getTrackCountDontUseThis();
                dest->reconcileTrackCount(Session::TypeAudio, oldTrackCount);
                
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
                writeSession(f, dest);
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
                Session* neu = transformer.setupToSession(setup);
                // former global parameters are duplicated in every session
                transformer.addGlobals(config, neu);
                createSession(neu);
                delete neu;
            }
            else {
                // we've already seen this one, normally would ignore it but
                // have a merge option just for testing
                bool testMerge = true;
                if (testMerge) {
                    Trace(2, "SessionClerk: Merging Setup %s", setup->getName());
                
                    Session* dest = readSession(f);
                    if (dest == nullptr) {
                        Trace(1, "SessionClerk: Unable to read Session to merge into %s", f->name.toUTF8());
                    }
                    else {
                        transformer.merge(setup, dest);
                        writeSession(f, dest);
                        delete dest;
                    }
                }
            }
        }
    }
}

/**
 * Given a Session fresly created from an old Setup, save it
 * in the library.
 */
void SessionClerk::createSession(Session* neu)
{
    juce::String name = neu->getName();
    
    if (!libraryValid) {
        Trace(1, "SessionClerk: Can't create session, invalid library");
    }
    else if (name.length() == 0) {
        Trace(1, "SessionClerk: Can't create session without name");
    }
    else {
        Folder* f = findFolder(name);
        if (f != nullptr) {
            // not expecting this here
            Trace(1, "SessionClerk: Session already exists %s", name.toUTF8());
        }
        else {
            juce::File dir = libraryRoot.getChildFile(name);
            juce::Result res = dir.createDirectory();
            if (res.failed()) {
                Trace(1, "SessionClerk: Unable to create session library folder");
                Trace(1, "  %s", res.getErrorMessage().toUTF8());
            }
            else {
                juce::File dest = dir.getChildFile("session.xml");
                dest.replaceWithText(neu->toXml());

                f = new Folder();
                f->name = name;
                f->path = dir.getFullPathName();
                f->valid = true;
                folders.add(f);
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
