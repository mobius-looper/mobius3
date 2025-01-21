
#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/Session.h"

#include "Provider.h"
#include "FileManager.h"

// stuff just for migration
#include "model/Symbol.h"
#include "model/ParameterProperties.h"
#include "model/ValueSet.h"
#include "model/MobiusConfig.h"
#include "model/Setup.h"
// why the fuck were using this and not Symbol?
#include "model/UIParameter.h"

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

        MobiusConfig* config = provider->getMobiusConfig();
        migrateSetups(config);
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

///////////////////////////////////////////////////////////////////////
//
// Migration
//
///////////////////////////////////////////////////////////////////////

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

/**
 * Early in startup, when the Producer is initialized, migrate Setup
 * objects from the old mobius.xml into Sessions.  This is only done once.
 *
 * Do NOT TOUCH mobius.xml.  It is imortant to be able to revert to an earlier
 * release that still uses it.
 */
void SessionClerk::migrateSetups(MobiusConfig* config)
{
    for (Setup* setup = config->getSetups() ; setup != nullptr ; setup = setup->getNextSetup()) {

        // !! this could be a problem if they used characters in setup names that are not
        // allowed in file names.  Need a name sanitizer
        Folder* f = findFolder(juce::String(setup->getName()));
        if (f != nullptr) {
            // already migrated or edited, ignore
        }
        else {
            Trace(2, "SessionClerk: Migrating Setup %s", setup->getName());

            Session* neu = new Session();
            neu->setName(juce::String(setup->getName()));

            migrateGlobals(config, neu);
            migrateSetup(setup, neu);
            
            createSession(neu);
        }
    }
}

void SessionClerk::migrateSetup(Setup* src, Session* dest)
{
    SymbolTable* symbols = provider->getSymbols();
    ValueSet*  values = dest->ensureGlobals();
    
    convertEnum(symbols->getName(ParamDefaultSyncSource), src->getSyncSource(), values);
    convertEnum(symbols->getName(ParamDefaultTrackSyncUnit), src->getSyncTrackUnit(), values);
    convertEnum(symbols->getName(ParamSlaveSyncUnit), src->getSyncUnit(), values);
    dest->setBool(symbols->getName(ParamManualStart), src->isManualStart());
    dest->setInt(symbols->getName(ParamMinTempo), src->getMinTempo());
    dest->setInt(symbols->getName(ParamMaxTempo), src->getMaxTempo());
    dest->setInt(symbols->getName(ParamBeatsPerBar), src->getBeatsPerBar());
    convertEnum(symbols->getName(ParamMuteSyncMode), src->getMuteSyncMode(), values);
    convertEnum(symbols->getName(ParamResizeSyncAdjust), src->getResizeSyncAdjust(), values);
    convertEnum(symbols->getName(ParamSpeedSyncAdjust), src->getSpeedSyncAdjust(), values);
    convertEnum(symbols->getName(ParamRealignTime), src->getRealignTime(), values);
    convertEnum(symbols->getName(ParamOutRealign), src->getOutRealignMode(), values);
    dest->setInt(symbols->getName(ParamActiveTrack), src->getActiveTrack());

    // these are all by definition Audio tracks
    // they will be numbered later if they are used and normalized
    SetupTrack* track = src->getTracks();
    while (track != nullptr) {
        Session::Track* strack = new Session::Track();
        strack->type = Session::TypeAudio;
        dest->add(strack);
        
        migrateTrack(track, strack);

        track = track->getNext();
    }
}

void SessionClerk::migrateTrack(SetupTrack* src, Session::Track* dest)
{
    SymbolTable* symbols = provider->getSymbols();
    ValueSet* values = dest->ensureParameters();
    
    dest->name = juce::String(src->getName());

    dest->setString(symbols->getName(ParamTrackName), src->getName());
    dest->setString(symbols->getName(ParamTrackPreset), src->getTrackPresetName());

    // why is this a parameter?  it's transient
    //dest->setString(ParamActivePreset, src->getActivePreset());

    dest->setBool(symbols->getName(ParamFocus), src->isFocusLock());

    // should have been upgraded to a name by now
    juce::String gname = src->getGroupName();
    if (gname.length() > 0)
      dest->setString(symbols->getName(ParamGroupName), gname.toUTF8());
        
    dest->setBool(symbols->getName(ParamMono), src->isMono());
    dest->setInt(symbols->getName(ParamFeedback), src->getFeedback());
    dest->setInt(symbols->getName(ParamAltFeedback), src->getAltFeedback());
    dest->setInt(symbols->getName(ParamInput), src->getInputLevel());
    dest->setInt(symbols->getName(ParamOutput), src->getOutputLevel());
    dest->setInt(symbols->getName(ParamPan), src->getPan());
    
    convertEnum(symbols->getName(ParamSyncSource), src->getSyncSource(), values);
    convertEnum(symbols->getName(ParamTrackSyncUnit), src->getSyncTrackUnit(), values);
        
    dest->setInt(symbols->getName(ParamAudioInputPort), src->getAudioInputPort());
    dest->setInt(symbols->getName(ParamAudioOutputPort), src->getAudioOutputPort());
    dest->setInt(symbols->getName(ParamPluginInputPort), src->getPluginInputPort());
    dest->setInt(symbols->getName(ParamPluginOutputPort), src->getPluginOutputPort());

    // these are defined as parameters but haven't been in the XML for some reason
    /*
    ParamSpeedOctave,
    ParamSpeedStep,
    ParamSpeedBend,
    ParamPitchOctave,
    ParamPitchStep,
    ParamPitchBend,
    ParamTimeStretch
    */
}    
    
/**
 * Convert the old global parameters into global session parameters.
 *
 * Note that this will duplicate them in EVERY Session, and once there
 * they will not be shared.  For most of them, this feels right.  A small
 * number of them might be REALLY global and should go in SystemConfig.
 *
 * Some of the things you may see in mobius.xml that are no longer used,
 * or were already converted to a new model like FunctionProperties
 * or GroupDefinition.
 *
 *    groupCount
 *    groupFocusLock
 *    driftCheckPoint
 *    focusLockFunctions
 *    muteCancelFunctions
 *    confirmationFunctions
 *    altFeedbackDisables
 *
 *    hostRewinds
 *    autoFeedbackReduction
 *    isolateOverdubs
 *    integerWaveFile
 *    tracePrintLevel
 *    traceDebugLevel
 *    saveLayers
 *    dualPluginWindow
 *    maxLayerInfo
 *    maxRedoInfo
 *    noSyncBeatRounding
 *    edpisms
 *
 */
void SessionClerk::migrateGlobals(MobiusConfig* src, Session* dest)
{
    SymbolTable* symbols = provider->getSymbols();
    ValueSet* values = dest->ensureGlobals();
    
    // this will have the effect of fleshing out SessionTrack objects
    // if there were not enough SetupTracks in the old file
    dest->audioTracks = src->getCoreTracks();
    
    dest->setInt(symbols->getName(ParamInputLatency), src->getInputLatency());
    dest->setInt(symbols->getName(ParamOutputLatency), src->getOutputLatency());
    dest->setInt(symbols->getName(ParamNoiseFloor), src->getNoiseFloor());
    dest->setInt(symbols->getName(ParamFadeFrames), src->getFadeFrames());
    dest->setInt(symbols->getName(ParamMaxSyncDrift), src->getNoiseFloor());
    dest->setInt(symbols->getName(ParamMaxLoops), src->getMaxLoops());
    dest->setInt(symbols->getName(ParamLongPress), src->getLongPress());
    dest->setInt(symbols->getName(ParamSpreadRange), src->getSpreadRange());
    dest->setInt(symbols->getName(ParamControllerActionThreshold), src->mControllerActionThreshold);
    dest->setBool(symbols->getName(ParamMonitorAudio), src->isMonitorAudio());
    dest->setString(symbols->getName(ParamQuickSave), src->getQuickSave());
    dest->setString(symbols->getName(ParamActiveSetup), src->getStartingSetupName());
    
    // enumerations have been stored with symbolic names, which is all we really need
    // but I'd like to test working backward to get the ordinals, need to streamline
    // this process
    convertEnum(symbols->getName(ParamDriftCheckPoint), src->getDriftCheckPoint(), values);
    convertEnum(symbols->getName(ParamMidiRecordMode), src->getMidiRecordMode(), values);
}

/**
 * Build the MslValue for an enumeration from this parameter name and ordinal
 * and isntall it in the value set.
 *
 * This does some consnstency checking that isn't necessary but I want to detect
 * when there are model inconsistencies while both still exist.
 */
void SessionClerk::convertEnum(juce::String name, int value, ValueSet* dest)
{
    // method 1: using UIParameter static objects
    // this needs to go away
    const char* cname = name.toUTF8();
    UIParameter* p = UIParameter::find(cname);
    const char* enumName = nullptr;
    if (p == nullptr) {
        Trace(1, "SessionClerk: Unresolved old parameter %s", cname);
    }
    else {
        enumName = p->getEnumName(value);
        if (enumName == nullptr)
          Trace(1, "SessionClerk: Unresolved old enumeration %s %d", cname, value);
    }

    // method 2: using Symbol and ParameterProperties
    Symbol* s = provider->getSymbols()->find(name);
    if (s == nullptr) {
        Trace(1, "SessionClerk: Unresolved symbol %s", cname);
    }
    else if (s->parameterProperties == nullptr) {
        Trace(1, "SessionClerk: Symbol not a parameter %s", cname);
    }
    else {
        const char* newEnumName = s->parameterProperties->getEnumName(value);
        if (newEnumName == nullptr)
          Trace(1, "SessionClerk: Unresolved new enumeration %s %d", cname, value);
        else if (enumName != nullptr && strcmp(enumName, newEnumName) != 0)
          Trace(1, "SessionClerk: Enum name mismatch %s %s", enumName, newEnumName);

        // so we can limp along, use the new name if the old one didn't match
        if (enumName == nullptr)
          enumName = newEnumName;
    }

    // if we were able to find a name from somewhere, install it
    // otherwise something was traced
    if (enumName != nullptr) {
        MslValue* mv = new MslValue();
        mv->setEnum(enumName, value);
        MslValue* existing = dest->replace(name, mv);
        if (existing != nullptr) {
            // first time shouldn't have these, anything interesting to say here?
            delete existing;
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
