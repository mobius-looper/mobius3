
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
        bootstrapDefaultSession();
        
        // convert old Setup objects into Sessions
        migrateSetups();
    }
}

/**
 * On a fresh install (or a corrupted install) if we don't
 * find the Default session in the library, attempt to create one.
 * During the period immediately after build 30, this will look for
 * session.xml in the root of the installation directory and copy it to the
 * new folder so we can retain the early settings without corrupting them.
 */
void SessionClerk::bootstrapDefaultSession()
{
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
        }
    }
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
 * Convert any Setup objects into Sessions.  This is normally done once.
 * Once a Session exists with the same name as a Setup, it is assumed that
 * the new session editor is being used and the old Setup objects can
 * be ignored.
 *
 * A special case exists for the Setup named "Default".  The Session with
 * that name is bootstrapped by copying the first prototype session.xml
 * into the session library folder and this is where Midi track definitions
 * for the earlier releases will live.  That session has no audio tracks.
 * When the Default Setup is encounered it needs to merge into the Default
 * Setup rather than creating a new one.
 *
 * For testing, this can also be done for all of them, but this is temporary.
 * Once the session editor is working properly it should never go back
 * to the Setups for audio tracks.
 *
 * NOTE: So that user can downgrade to earlier builds, it is important that
 * we NOT TOUCH either mobius.xml or the original session.xml.
 */
void SessionClerk::migrateSetups()
{
    ModelTransformer transformer(provider);
    
    MobiusConfig* config = provider->getMobiusConfig();
    for (Setup* setup = config->getSetups() ; setup != nullptr ; setup = setup->getNextSetup()) {

        juce::String setupName (setup->getName());
        Folder* f = findFolder(setupName);

        if (f == nullptr) {

            // this should not be happening if bootstrapDefaultSession worked
            if (setupName == "Default")
              Trace(1, "SessionClerk: Migrating into Default, bootstrap failed");
            else
              Trace(2, "SessionClerk: Migrating Setup %s", setup->getName());
            
            Session* neu = transformer.setupToSession(setup);
            // former global parameters are duplicated in every session
            transformer.addGlobals(config, neu);
            createSession(neu);
            delete neu;
        }
        else {
            bool merge = (setupName == "Default");
            // temporary testing
            merge = true;
            
            if (merge) {
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
