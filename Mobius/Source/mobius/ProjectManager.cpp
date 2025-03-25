/**
 * Utility class to organize the project loading and saving process.
 *
 * Not happy with the layering here, but this gets it going.
 * What I'd like is to have MobiusInterface receive and return standalone
 * Project objects and push all file handling up to the UI where we can play around
 * with how these are packaged since project files and the emergine "session"
 * concept are going to be closely related.
 *
 * The main sticking point is the Audio objects which we have right now
 * for loadLoop.  These like to use an AudioPool embedded deep within the
 * engine so it's easier if we deal with the model in the shell.
 *
 * Once Audio/AudioPool get redesigned it will be cleaner.
 */

#include <stdio.h>

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../util/Util.h"
#include "../util/XmlBuffer.h"
#include "../util/XomParser.h"
#include "../util/List.h"

#include "core/Project.h"
#include "core/Mobius.h"
#include "core/Track.h"
#include "core/Loop.h"

#include "Audio.h"
#include "AudioFile.h"

#include "MobiusShell.h"
#include "ProjectManager.h"

ProjectManager::ProjectManager(MobiusShell* parent)
{
    shell = parent;
}

ProjectManager::~ProjectManager()
{
}

//////////////////////////////////////////////////////////////////////
//
// Save
//
//////////////////////////////////////////////////////////////////////

/**
 * Main entry point to save projects.
 */
juce::StringArray ProjectManager::saveProject(juce::File file)
{
    errors.clear();

    Trace(2, "ProjectManager: Saving project");

    // old code went in two steps
    // first Mobius::saveProject created a Project object
    // then Project::write wrote the files
    // this has been repackaged as saveProject and writeProject below

    Project* p = saveProject();
    writeProject(p, file.getFullPathName().toUTF8(), false);

    return errors;
}

/**
 * This starts the adaptation of the original Mobius::saveProject
 *
 * old comments:
 * Capture the state of the Mobius in a Project.
 * Tried to do this in the interupt handler, but if we have to flatten
 * layers it's too time consuming.  In theory we could have contention
 * with functions being applied while the save is in progress, but 
 * that would be rare.  
 * !! At least ensure that we won't crash.
 * 
 * Note that we're getting copies of Audio objects that are still
 * technically owned by the Layers.  As long as you save the project
 * before any radical change, like a Reset, it will be ok.  But if
 * you Reset or TrackReset and start recording a new loop before
 * the Project is saved, the Audio's that end up being saved may
 * not be what you started with.  
 *
 * The most important thing is that they remain valid heap objects, which
 * will be true since we always pool Layer objects.  So, while you
 * may get the wrong audio, you at least won't crash.  
 *
 * Providing an absolutely accurate snapshot requires that we make a copy
 * of all the Audio objects when building the Project, this may be
 * a very expensive operation which would cause us to miss interrupts.
 * 
 * So, we compromise and give you pointers to "live" objects that will
 * usuallly remain valid until the project is saved.  The only time
 * the objects would be modified is if you were running a script that
 * didn't wait for the save, or if you were using MIDI control at the
 * same time you were saving the project.  Both are unlikely and avoidable.
 */
Project* ProjectManager::saveProject()
{
    Project* p = new Project();

    if (shell->suspendKernel()) {
        // someday try out the newer style of auto-destricting resource
        // managers like juce::CriticalSection does

        Mobius* core = shell->getKernel()->getCore();

        // overlay bindings don't make sense any more at this level but
        // it does highlight how an old Project was more than just Track state,
        // it also captured some of the UI state
#if 0
        BindingConfig* overlay = mConfig->getOverlayBindingConfig();
        if (overlay != nullptr)
          p->setBindings(overlay->getName());

        Setup* s = core->getSetup();
        p->setSetup(s->getName());
#endif

        // the old Project model does all the work here
        p->setTracks(core);
        
        shell->resumeKernel();
    }
    
    // a finished flag was important for some reason, unclear why
    // probably thread phasing
	//p->setFinished(true);

    return p;
}
//
// This is the old code that handled the menu items
// There were two variants, the project and the template
// The menu item for SAVE_TEMPLATE was commented out and I don't remember
// what the intent was, but it was probably more like a "session" with
// track configuration but no loop/layer content
//
// It first used Mobius::saveProject to build the Project object
// Then called Project::write on the chosen file
//
#if 0
		else if (id == IDM_SAVE_PROJECT || id == IDM_SAVE_TEMPLATE) {
			bool isTemplate = (id == IDM_SAVE_TEMPLATE);

			sprintf(filter, "%s (.mob)|*.mob", 
					cat->get(MSG_DLG_SAVE_PROJECT_FILTER));

			OpenDialog* od = new OpenDialog(mWindow);
			od->setSave(true);
			if (isTemplate)
			  od->setTitle(cat->get(MSG_DLG_SAVE_TEMPLATE));
			else
			  od->setTitle(cat->get(MSG_DLG_SAVE_PROJECT));
			od->setFilter(filter);
			showSystemDialog(od);
			if (!od->isCanceled()) {
				Project* p = mMobius->saveProject();
				if (!p->isError()) {
                    const char* file = od->getFile();
					p->write(file, isTemplate);
                    // might have write errors
					if (p->isError())
					  alert(p->getErrorMessage());
				}
				else {
					// some errors are worse than others, might want
					// an option to load what we can 
					alert(p->getErrorMessage());
				}
                delete p;
			}
			delete od;
		}
#endif

/**
 * From here on down is a repackaging of Project::write, ProjectTrack::writeAudio,
 * ProjectLoop::writeAudio, ProjectLayer::writeAudio, and Audio::write
 */
void ProjectManager::writeProject(Project* p, const char* file, bool isTemplate)
{
	char path[1024];
	char baseName[1024];

    // don't need these, just accumulate things in the errors list
	//mError = false;
	//strcpy(mMessage, "");

    // here we were adding the .mob extension if you chose a file that didn't
    // have one, unclear if that can happen with the new file chooser
	if (EndsWithNoCase(file, ".mob"))
	  strcpy(path, file);
	else
	  snprintf(path, sizeof(path), "%s.mob", file);

	// calculate the base file name to be used for Audio files
	strcpy(baseName, path);
	int psn = LastIndexOf(baseName, ".");
	if (psn > 0)
	  baseName[psn] = 0;

	// clean up existing Audio files
    FILE* fp = fopen(path, "r");
    if (fp != nullptr) {
        fclose(fp);

        // holy shit, what does this do?  I think just reads the XML structure
        Project* existing = new Project(path);
        deleteAudioFiles(existing);
        delete existing;
    }

	// write the new project and Audio files
	fp = fopen(path, "w");
	if (fp == nullptr) {
        // try multiple alert lines...
        // errors.add(juce::String("Unable to open output file: ") + juce::String(path));
        errors.add("Unable to open project file");
        errors.add(juce::String(path));
    }
	else {
		fclose(fp);

		// first write Audio files and assign Layer paths
        if (!isTemplate)
          writeAudio(p, baseName);
			
        // then write the XML directory
        XmlBuffer* b = new XmlBuffer();
        p->toXml(b, isTemplate);
        WriteFile(path, b->getString());
        delete b;
	}
}

void ProjectManager::writeAudio(Project* p, const char* baseName)
{
    List* tracks = p->getTracks();
    if (tracks != nullptr) {
		for (int i = 0 ; i < tracks->size() ; i++) {
			ProjectTrack* track = (ProjectTrack*)tracks->get(i);
			writeAudio(track, baseName, i + 1);
		}
    }
}

void ProjectManager::writeAudio(ProjectTrack* track, const char* baseName, int tracknum)
{
    List* loops = track->getLoops();
	if (loops != nullptr) {
		for (int i = 0 ; i < loops->size() ; i++) {
			ProjectLoop* loop = (ProjectLoop*)loops->get(i);
			writeAudio(loop, baseName, tracknum, i + 1);
		}
	}
}

void ProjectManager::writeAudio(ProjectLoop* loop, const char* baseName, int tracknum, int loopnum)
{
    List* layers = loop->getLayers();
	if (layers != nullptr) {
		for (int i = 0 ; i < layers->size() ; i++) {
			ProjectLayer* layer = (ProjectLayer*)layers->get(i);
			// use the layer id, it makes more sense
			//int layernum = i + 1;
			int layernum = layer->getId();
			writeAudio(layer, baseName, tracknum, loopnum, layernum);
		}
	}
}

void ProjectManager::writeAudio(ProjectLayer* layer, const char* baseName,
                                int tracknum, int loopnum, int layernum)
{
	char path[1024];

    Audio* audio = layer->getAudio();

    if (audio != nullptr && !audio->isEmpty() && !layer->isProtected()) {

        // todo: need to support inline audio in the XML
        snprintf(path, sizeof(path), "%s-%d-%d-%d.wav", baseName, 
				tracknum, loopnum, layernum);

        // Remember the new path too, should we every try to reuse
        // the previous path?  could be out of order by now
        layer->setPath(path);

        writeAudio(audio, path);
    }

    Audio* overdub = layer->getOverdub();
	if (overdub != nullptr && !overdub->isEmpty()) {
        // todo: need to support inline audio in the XML
        snprintf(path, sizeof(path), "%s-%d-%d-%d-overdub.wav", baseName, 
				tracknum, loopnum, layernum);
		layer->setOverdubPath(path);
		writeAudio(overdub, path);
	}
}

/**
 * This would have been Audio::write
 * There were three variants:
 *    write(path)
 *    write(path, format)
 *    write(path, format, frames)
 *
 */
void ProjectManager::writeAudio(Audio* audio, const char* path)
{
    // this was repackaged awhile ago into this but it doesn't
    // support accumulation of error message, need to fix that!!

    // when exchanging project files with other applications it can
    // be important to save the correct sample rate used when they were
    // recorded.  AudioFile will take the sampleRate stored in the Audio
    // object
    audio->setSampleRate(shell->getContainer()->getSampleRate());

    errors.addArray(AudioFile::write(juce::File(path), audio));
}

/**
 * Delete all of the external layer files associated with 
 * this project.  This is called prior to saving a project so 
 * we make sure to clean out old layer files that are no longer
 * relevant to the project.
 *
 * In case the project was hand written and included references
 * to files outside the project directory, ignore those.
 *
 * !! Don't see the logic to protected external files
 *
 * new: Among the other things that suck, is that the absolute paths are embedded
 * with in the Project model so relocating them would appear not to work.
 * this sucks mightily
 */
void ProjectManager::deleteAudioFiles(Project* p)
{
    List* tracks = p->getTracks();
    if (tracks != nullptr) {
        for (int i = 0 ; i < tracks->size() ; i++) {
            ProjectTrack* track = (ProjectTrack*)tracks->get(i);
            List* loops = track->getLoops();
            if (loops != nullptr) {
                for (int j = 0 ; j < loops->size() ; j++) {
                    ProjectLoop* loop = (ProjectLoop*)loops->get(j);
                    List* layers = loop->getLayers();
                    if (layers != nullptr) {
                        for (int k = 0 ; k < layers->size() ; k++) {
                            ProjectLayer* layer = (ProjectLayer*)layers->get(k);
                            const char* path = layer->getPath();
                            if (path != nullptr && !layer->isProtected()) {
                                FILE* fp = fopen(path, "r");
                                if (fp != nullptr) {
                                    fclose(fp);
                                    remove(path);
                                }
                                else {
                                    // should be returning messages if we couldn't delete files
                                    // should this hold up saving the rest of the project though?
                                }
                            }
                            path = layer->getOverdubPath();
                            if (path != nullptr) {
                                FILE* fp = fopen(path, "r");
                                if (fp != nullptr) {
                                    fclose(fp);
                                    remove(path);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Load
//
//////////////////////////////////////////////////////////////////////

//
// Old code for the Open Project menu item
// The steps here were to create a Project(path) then call Project::read(pool).
// At that point a Project is in memory full of Audio.
// If there were no errors call Mobius::loadProject
// 
#if 0
		else if (id == IDM_OPEN_PROJECT) {

			sprintf(filter, "%s (.mob)|*.mob;*.MOB", 
					cat->get(MSG_DLG_OPEN_PROJECT_FILTER));

			OpenDialog* od = new OpenDialog(mWindow);
			od->setTitle(cat->get(MSG_DLG_OPEN_PROJECT));
			od->setFilter(filter);
            showSystemDialog(od);
			if (!od->isCanceled()) {
				const char* file = od->getFile();

                // it would encapsulate AudioPool if we had
                // Mobius::loadProject(const char* file)

                AudioPool* pool = mMobius->getAudioPool();
                Project* p = new Project(file);
                p->read(pool);

				if (!p->isError())
				  mMobius->loadProject(p);
				else {
					// some errors are worse than others, might want
					// an option to load what we can 
					alert(p->getErrorMessage());
				}

				mWindow->invalidate();
			}
			delete od;

		}
#endif

/**
 * This one is relatively simple.
 * 
 * We first read the Project XML file which builds out
 * the structure and the Project classes do the parsing.
 *
 * Next we walk over that until we reach ProjectLayers that
 * have paths to audio files.  The Audio objects are read and
 * left in the Project.
 */
juce::StringArray ProjectManager::loadProject(juce::File file)
{
    errors.clear();

    Trace(2, "ProjectManager: Loading project");

    // this just creates a Project object and copies the path
    // probably not necessary since reading was moved outside
    // well no, the path is saved in the project's XML so it might
    // have been necessary on read
    // !! remove references to paths within the project XML
    // the only one that looks like it might be harder is layer
    // everything else should be able to start from the file we
    // browsed to
    Project* p = new Project(file.getFullPathName().toUTF8());

    // set this so we can find .wav files relative to the .mob file
    // later in readAudio
    projectRoot = file.getParentDirectory();

    // formerly Project::read(pool)
    read(p, file);

    
    if (errors.size() > 0) {
        // should have already trace something, caller responsible for
        // alerting about the errors
        // interesting that old code never deleted the Project, probably a leak
        // have to watch this careful to make sure the Audio is returned to the pool
        delete p;
    }
    else {
        // this really needs an RAII kernel resumer object like
        // juce::CriticalSection
        if (shell->suspendKernel()) {
            // this does the dowward walk and deletes the passed Project when
            // it finishes
            Mobius* core = shell->getKernel()->getCore();
            core->loadProject(p);

            shell->resumeKernel();
        }
        else {
            // couldn't suspend, don't leak
            delete p;
        }
    }

    return errors;
}

// From here on down starts a repacking of Project::read

void ProjectManager::read(Project* p, juce::File file)
{
    // let the old file utils take over
    // should have an extension by now
    const char* path = file.getFullPathName().toUTF8();
    
	FILE* fp = fopen(path, "r");
	if (fp == nullptr) {
        // errors.add(juce::String("Unable to open file: ") + path);
        errors.add("Unable to open project file");
        errors.add(path);
	}
	else {
		fclose(fp);
        
        XomParser* parser = new XomParser();
        XmlDocument* d = parser->parseFile(path);
        if (d != nullptr) {
            XmlElement* e = d->getChildElement();
            if (e != nullptr) {

                // I guess if it had previously been read, clean it out
                p->clear();
                // build out internal structure from XML
                p->parseXml(e);
            }
            delete d;
        }
        else {
            // there was a syntax error in the file
            // these can be long so let's make use of the multi-line AlertPanel
            //snprintf(mMessage, sizeof(mMessage), "Unable to read file %s: %s\n", 
            //path, p->getError());
            //mError = true;
            //errors.add(juce::String("Unable to read file ") +
            //juce::String(path) + ": " +
            //juce::String(parser->getError()));
            errors.add("Unable to read project file");
            errors.add(path);
            errors.add(parser->getError());
        }
        delete parser;

        // now that the structure has been filled out, walk over
        // it until we find audio files to read
        if (errors.size() == 0)
          readAudio(p);
    }
}

void ProjectManager::readAudio(Project* p)
{
    List* tracks = p->getTracks();
    if (tracks != nullptr) {
        for (int i = 0 ; i < tracks->size() && errors.size() == 0 ; i++) {
            ProjectTrack* track = (ProjectTrack*)tracks->get(i);
            List* loops = track->getLoops();
            if (loops != nullptr) {
                for (int j = 0 ; j < loops->size() && errors.size() == 0 ; j++) {
                    ProjectLoop* loop = (ProjectLoop*)loops->get(j);
                    List* layers = loop->getLayers();
                    if (layers != nullptr) {
                        for (int k = 0 ; k < layers->size() && errors.size() == 0 ; k++) {
                            ProjectLayer* layer = (ProjectLayer*)layers->get(k);

                            // now the meat
                            const char* path = layer->getPath();
                            if (path != nullptr)
							  layer->setAudio(readAudio(path));

                            if (errors.size() == 0) {
                                path = layer->getOverdubPath();
                                if (path != nullptr)
                                  layer->setOverdub(readAudio(path));
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * This was formerly represented like this:
 *  layer->setAudio(pool->newAudio(path));
 *
 * AudioPool no longer has a newAudio method that takes a path
 * so we read it in a different way.
 */
Audio* ProjectManager::readAudio(const char* path)
{
    // have a new tool for this with the old code packaged
    // with juce::File

    // .mob files have historically used absolute paths but it is
    // quite common for those to be wrong when changing machines
    // or exchanging projects with someone else
    // I think I'd like to enforce that the .wav files be relative
    // to the .mob file, though this may break old projects where the user
    // manually moved the files somewhere else and edited the .mob file
    // unlikely though

    // this doesn't seem to do enough, was hoping it would mutate slashes but
    // it doesn't
    juce::String convpath =  juce::File::createLegalPathName(path);

    // hack, if the path already appears to be relative, don't stick it
    // in a juce::File since juce will break with an annoying assertion
    juce::File file;
    if (looksAbsolute(convpath)) {
        juce::File jpath = juce::File(convpath);
        // todo: here would be an option to preserve the original path, or redirect
        // ito the project root
        file = projectRoot.getChildFile(jpath.getFileName());
    }
    else {
        file = projectRoot.getChildFile(convpath);
    }

    AudioPool* pool = shell->getAudioPool();
    return AudioFile::read(file, pool, errors);
}

/**
 * Return true if this smells like an absolute path so we can avoid
 * an annoying Juce assertion if you try to construct a juce::File with
 * a relative path.
 *
 * On mac, this would start with '/'
 * On windows, this would have ':' in it somewhere, typically
 * as the second character but I guess it doesn't matter.
 *
 * I suppose we could be more accurate by looking at the actual runtime
 * architecture.  It actually would be nice in my testing to auto-convert
 * from one style to another so files in source control like mobius.xml ScriptConfig
 * can easly slide between them.
 * 
 */
bool ProjectManager::looksAbsolute(juce::String path)
{
    return path.startsWithChar('/') || path.containsChar(':');
}

//////////////////////////////////////////////////////////////////////
//
// Old File Utilities
//
//////////////////////////////////////////////////////////////////////

int ProjectManager::WriteFile(const char* name, const char* content) 
{
	int written = 0;

	FILE* fp = fopen(name, "wb");
	if (fp != nullptr) {
		if (content != nullptr)
		  written = (int)fwrite(content, 1, strlen(content), fp);
		fclose(fp);
	}
	return written;
}

//////////////////////////////////////////////////////////////////////
//
// Individual Loop Load
//
// This is the old implementation that loaded loops by making a special
// "incremental" project and loading the project.  It is different
// than what AudioClerk now does with drag and drop.
// Don't need this, but get it working to see if it adds anything useful.
//
//////////////////////////////////////////////////////////////////////

// old code from the menu
// it reads the Audio from the selected file then calls Mobius::loadLoop

#if 0
		else if (id == IDM_OPEN_LOOP) {

			sprintf(filter, "%s (.wav)|*.wav;*.WAV", 
					cat->get(MSG_DLG_OPEN_LOOP_FILTER));

			OpenDialog* od = new OpenDialog(mWindow);
			od->setTitle(cat->get(MSG_DLG_OPEN_LOOP));
			od->setFilter(filter);
            showSystemDialog(od);
			if (!od->isCanceled()) {
				const char* file = od->getFile();
                AudioPool* pool = mMobius->getAudioPool();
				Audio* au = pool->newAudio(file);
				mMobius->loadLoop(au);
				// loop meter is sensitive to this, maybe others
				mSpace->invalidate();
			}
			delete od;
		}

// Mobius::loadLoop then did this
PUBLIC void Mobius::loadLoop(Audio* a)
{
    if (mTrack != nullptr) {
        Loop* loop = mTrack->getLoop();
        // sigh, Track number is zero based, Loop number is one based
        Project* p = new Project(a, mTrack->getRawNumber(), loop->getNumber() - 1);
        // this causes it to merge rather than reset
        p->setIncremental(true);

        loadProject(p);
    }
}
#endif

// what that accomplshed was to create a Project with a single
// ProjectTrack and a single ProjectLoop and and gave them the numbers
// for the currently active track loop.  So the loop  would go into the one
// currently active.  That's kind of interesting, and it may be useful
// to be able to pass more information through in the wrapper Project than
// just the layer audio.
//
// Project really could be (if it wasn't already) a very targeted way to
// set content for specific tracks and loops, with or without layers.

/**
 * This then is about what the old Mobius::loadLoop did
 */
juce::StringArray ProjectManager::loadLoop(juce::File file)
{
    errors.clear();

    Trace(2, "ProjectManager: Loading loop");

    // before we bother reading the file, make sure it can go somewhere
    // this is relatively safe to do without suspending the kernel
    // the danger would be a pending MobiusConfig update that reduces
    // the track or loop count under what we scrape right now

    // rethink the interface here, I like being able to build the Project
    // outside the core, but to get it numbered reliably we need to ask the
    // core where it is, I guess you could get that from SystemState

    Mobius* core = shell->getKernel()->getCore();
    // will always have an active track
    Track* track = core->getTrack();
    int trackIndex = track->getRawNumber();

    Loop* loop = track->getLoop();
    // unclear whether tracks can be empty without loops, probably not
    if (loop == nullptr) {
        Trace(1, "ProjectManager: Attempt to load loop into empty track");
    }
    else {

        // read the audio file
        AudioPool* pool = shell->getAudioPool();
        Audio* audio = AudioFile::read(file, pool, errors);

        if (errors.size() == 0) {
            // this had a special constructor just for this purpose
            // note that loop->getNumber() is one based, and here we need the index
            Project* p = new Project(audio, trackIndex, loop->getNumber() - 1);
            // the magic beans
            p->setIncremental(true);

            // now we need to suspend, may as well have done it to include the
            // index probes above
            if (shell->suspendKernel()) {

                core->loadProject(p);

                shell->resumeKernel();
            }
            else {
                // yuno suspend?
                delete p;
                errors.add("Unable to suspend Kernel");
            }
        }
    }
    
    return errors;
}

//////////////////////////////////////////////////////////////////////
//
// Individual Loop Save
//
// This did not use Project, it used Mobius::getPlaybackAudio and
// wrote the file.  It could only target the active loop in the active track.
// getPlaybackAudio still exists, and there is some newer machinery around
// that that uses KernelCommunicator (maybe), I think it was TestDriver
// so it was going direct too.
//
// Again, get it working, just so people can make progress the old way
// but need to redesign all this to be cleaner.
//
// There is already a mechanism for save loops for the TestDriver
// but we're not using that for the menus.  Need to consolidate these!!
//
//
//////////////////////////////////////////////////////////////////////

/**
 * This is approximately what Mobius::saveLoop did, except that redirected
 * through MobiusThread for some reason.  Simplified to assume that
 * the .wav extension was already put on, and that quicksave path
 * derivation was already done.
 */
juce::StringArray ProjectManager::saveLoop(juce::File file)
{
    errors.clear();
    
    Trace(2, "ProjectManager: Saving loop");
    
    // hmm, for quicksave in particular it is really nice not to suspend
    // the kernel which causes a glitch
    // this is a lot less complicated than Projects, and I THINK it
    // can be done reliably on a live engine, at least we did it that
    // way in the past, will really depend on how active the user
    // is, but they really shouldn't be if they're bothering with
    // the quick save menu/button
    // if this isn't reliable then we need to find a way to make it reliable
    // can push it into the Kernel with a KernelMessage but then Kernel does
    // a few memory allocations which isn't that bad considering
    bool fastAndLoose = true;

    if (fastAndLoose) {
        Mobius* core = shell->getKernel()->getCore();
        Audio* audio = core->getPlaybackAudio();
        int sampleRate = shell->getContainer()->getSampleRate();
        errors.addArray(AudioFile::write(file, audio, sampleRate));
        delete audio;
    }
    else {
        if (shell->suspendKernel()) {

            Mobius* core = shell->getKernel()->getCore();
            Audio* audio = core->getPlaybackAudio();
            int sampleRate = shell->getContainer()->getSampleRate();
            errors.addArray(AudioFile::write(file, audio, sampleRate));
            // this was a flattened copy of the play layer and must be reclaimed
            delete audio;

            // todo: capture errors

            // note that Audio is still owned by the core, you do not delete it

            // this is where this really should use an RAII resource reclaimer
            // since it is more likely for AudioFile::write to throw an exception
            // than it is for other uses of kernel suspend
            shell->resumeKernel();
        }
        else {
            errors.add("Unable to suspend Kernel");
        }
    }
    
    return errors;
}

/**
 * Newer interface, primarily for saving drag-and-drop temp files.
 * Here the track number and loop number are specified, which is what
 * normal saveLoop should be doing too.
 */
juce::StringArray ProjectManager::saveLoop(int trackNumber, int loopNumber, juce::File& file)
{
    errors.clear();

    // don't mess around with fastAndLoose mode for this one
    if (shell->suspendKernel()) {

        MobiusKernel* kernel = shell->getKernel();
        errors = kernel->saveLoop(trackNumber, loopNumber, file);
        
        shell->resumeKernel();
    }
    else {
        errors.add("Unable to suspend Kernel");
    }

    return errors;
}



/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
