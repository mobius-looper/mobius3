
#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/Session.h"

#include "Provider.h"
#include "FileManager.h"

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
    else {
        if (!sessions.isDirectory()) {
            juce::Result res = sessions.createDirectory();
            if (res.failed()) {
                Trace(1, "SessionClerk: Unable to create sessions library folder");
                Trace(1, "  %s", res.getErrorMessage().toUTF8());
                thisIsFine = false;
            }
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

            // todo: need to look inside
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

//////////////////////////////////////////////////////////////////////
//
// Session Files
//
//////////////////////////////////////////////////////////////////////

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

                // it doesn't matter what the .xml file had for name, it gets where it was
                session->setName(f->name);
            }
        }
    }
    return session;
}

void SessionClerk::writeSession(Folder* f, Session* s)
{
    if (f != nullptr) {
        juce::File root (f->path);
        juce::File dest = root.getChildFile("session.xml");

        // this doesn't matter, but make sure it matches to avoid confusion
        s->setName(f->name);
        
        dest.replaceWithText(s->toXml());
    }
}

//////////////////////////////////////////////////////////////////////
//
// Default Session
//
//////////////////////////////////////////////////////////////////////

/**
 * On a fresh install (or a corrupted install) if we don't
 * find the Default session in the library, attempt to create one.
 * During the period immediately after build 30, this will look for
 * session.xml in the root of the installation directory and copy it to the
 * new folder so we can retain the early settings without corrupting them.
 */
SessionClerk::Folder* SessionClerk::bootstrapDefaultSession()
{
    Folder* result = nullptr;

    if (libraryValid) {
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
                
            result = new Folder();
            result->name = "Default";
            result->path = sessionRoot.getFullPathName();
            result->valid = true;
            
            folders.add(result);
        }
    }
    return result;
}

/**
 * This one is special, we don't bail if it doesn't exist.
 */
Session* SessionClerk::readDefaultSession()
{
    Session* session = nullptr;

    if (libraryValid) {
        Folder* f = findFolder(juce::String("Default"));
        if (f == nullptr)
          f = bootstrapDefaultSession();
        session = readSession(f);
    }

    // prevent NPEs
    if (session == nullptr) {
        Trace(1, "SessionClerk: Unable to read default session, you will lose");
        session = new Session();
    }
    
    return session;
}

//////////////////////////////////////////////////////////////////////
//
// Public Interface
//
//////////////////////////////////////////////////////////////////////

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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
