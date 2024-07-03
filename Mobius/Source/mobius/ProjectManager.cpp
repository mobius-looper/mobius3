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
juce::StringArray ProjectManager::save(juce::File file)
{
    errors.clear();

    // magic
    Trace(2, "ProjectManager::save got all the way here with %s",
          file.getFullPathName().toUTF8());

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
        if (overlay != NULL)
          p->setBindings(overlay->getName());
#endif

        Setup* s = core->getSetup();
        p->setSetup(s->getName());

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
    if (fp != NULL) {
        fclose(fp);

        // holy shit, what does this do?  I think just reads the XML structure
        Project* existing = new Project(path);
        deleteAudioFiles(existing);
        delete existing;
    }

	// write the new project and Audio files
	fp = fopen(path, "w");
	if (fp == NULL) {
		//snprintf(mMessage, sizeof(mMessage), "Unable to open output file: %s\n", path);
		//mError = true;
        errors.add(juce::String("Unable to open output file: ") + juce::String(path));
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
#if 0    
	if (mTracks != NULL) {
		for (int i = 0 ; i < mTracks->size() ; i++) {
			ProjectTrack* track = (ProjectTrack*)mTracks->get(i);
			track->writeAudio(baseName, i + 1);
		}
	}
#endif
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
	if (loops != NULL) {
		for (int i = 0 ; i < loops->size() ; i++) {
			ProjectLoop* loop = (ProjectLoop*)loops->get(i);
			writeAudio(loop, baseName, tracknum, i + 1);
		}
	}
}

void ProjectManager::writeAudio(ProjectLoop* loop, const char* baseName, int tracknum, int loopnum)
{
    List* layers = loop->getLayers();
	if (layers != NULL) {
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

    if (audio != NULL && !audio->isEmpty() && !layer->isProtected()) {

        // todo: need to support inline audio in the XML
        sprintf(path, "%s-%d-%d-%d.wav", baseName, 
				tracknum, loopnum, layernum);

        // Remember the new path too, should we every try to reuse
        // the previous path?  could be out of order by now
        layer->setPath(path);

        writeAudio(audio, path);
    }

    Audio* overdub = layer->getOverdub();
	if (overdub != NULL && !overdub->isEmpty()) {
        // todo: need to support inline audio in the XML
        sprintf(path, "%s-%d-%d-%d-overdub.wav", baseName, 
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

    AudioFile::write(juce::File(path), audio);
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
    if (tracks != NULL) {
        for (int i = 0 ; i < tracks->size() ; i++) {
            ProjectTrack* track = (ProjectTrack*)tracks->get(i);
            List* loops = track->getLoops();
            if (loops != NULL) {
                for (int j = 0 ; j < loops->size() ; j++) {
                    ProjectLoop* loop = (ProjectLoop*)loops->get(j);
                    List* layers = loop->getLayers();
                    if (layers != NULL) {
                        for (int k = 0 ; k < layers->size() ; k++) {
                            ProjectLayer* layer = (ProjectLayer*)layers->get(k);
                            const char* path = layer->getPath();
                            if (path != NULL && !layer->isProtected()) {
                                FILE* fp = fopen(path, "r");
                                if (fp != NULL) {
                                    fclose(fp);
                                    remove(path);
                                }
                            }
                            path = layer->getOverdubPath();
                            if (path != NULL) {
                                FILE* fp = fopen(path, "r");
                                if (fp != NULL) {
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
juce::StringArray ProjectManager::load(juce::File file)
{
    errors.clear();

    // magic
    Trace(2, "ProjectManager::load got all the way here with %s",
          file.getFullPathName().toUTF8());

    // repackaging of the old menu handler

    // this just creates a Project object and copies the path
    // probably not necessary since reading was moved outside
    // well no, the path is saved in the project's XML so it might
    // have been necessary on read
    // !! remove references to paths within the project XML
    // the only one that looks like it might be harder is layer
    // everything else should be able to start from the file we
    // browsed to
    Project* p = new Project(file.getFullPathName().toUTF8());

    // formerly Project::read(pool)
    read(p, file);
    
    if (p->isError()) {
        // todo: this is where it would do an alert
        // find out where the errors were left and move them to the error list
        Trace(1, "ProjectManager: Need a read alert here!");
        
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
#if 0
	char path[1024];
	mError = false;
	strcpy(mMessage, "");

	if (strchr(file, '.') != NULL)
	  strcpy(path, file);
	else {
		// auto extend
		snprintf(path, sizeof(path), "%s.mob", file);
	}
#endif

    // let the old file utils take over
    // should have an extension by now
    const char* path = file.getFullPathName().toUTF8();
    
	FILE* fp = fopen(path, "r");
	if (fp == NULL) {
		//snprintf(mMessage, sizeof(mMessage), "Unable to open file %s\n", path);
        errors.add(juce::String("Unable to open file: ") + path);
		//mError = true;
	}
	else {
		fclose(fp);
        
        XomParser* parser = new XomParser();
        XmlDocument* d = parser->parseFile(path);
        if (d != NULL) {
            XmlElement* e = d->getChildElement();
            if (e != NULL) {

                // I guess if it had previously been read, clean it out
                p->clear();
                // build out internal structure from XML
                p->parseXml(e);
            }
            delete d;
        }
        else {
            // there was a syntax error in the file
            //snprintf(mMessage, sizeof(mMessage), "Unable to read file %s: %s\n", 
            //path, p->getError());
            //mError = true;
            errors.add(juce::String("Unable to read file ") +
                       juce::String(path) + ": " +
                       juce::String(parser->getError()));
        }
        delete parser;

        // now that the structure has been filled out, walk over
        // it until we find audio files to read
        readAudio(p);
    }
}

void ProjectManager::readAudio(Project* p)
{
    List* tracks = p->getTracks();
    if (tracks != nullptr) {
        for (int i = 0 ; i < tracks->size() ; i++) {
            ProjectTrack* track = (ProjectTrack*)tracks->get(i);
            List* loops = track->getLoops();
            if (loops != NULL) {
                for (int j = 0 ; j < loops->size() ; j++) {
                    ProjectLoop* loop = (ProjectLoop*)loops->get(j);
                    List* layers = loop->getLayers();
                    if (layers != NULL) {
                        for (int k = 0 ; k < layers->size() ; k++) {
                            ProjectLayer* layer = (ProjectLayer*)layers->get(k);

                            // now the meat
                            const char* path = layer->getPath();
                            if (path != NULL)
							  layer->setAudio(readAudio(path));
                            
                            path = layer->getOverdubPath();
                            if (path != NULL)
							  layer->setOverdub(readAudio(path));
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

    AudioPool* pool = shell->getAudioPool();
    return AudioFile::read(juce::File(path), pool);
}

//////////////////////////////////////////////////////////////////////
//
// Old File Utilities
//
//////////////////////////////////////////////////////////////////////

// already had these in Util.h
#if 0
bool EndsWithNoCase(const char* str, const char* suffix)
{
	bool endsWith = false;
	if (str != NULL && suffix != NULL) {
		size_t len1 = strlen(str);
		size_t len2 = strlen(suffix);
		if (len1 > len2)
		  endsWith = StringEqualNoCase(suffix, &str[len1 - len2]);
	}
	return endsWith;
}

int LastIndexOf(const char* str, const char* substr)
{
	int index = -1;

	// not a very smart search
	if (str != NULL && substr != NULL) {
		size_t len = strlen(str);
		size_t sublen = strlen(substr);
		size_t psn = len - sublen;
		if (psn >= 0) {
			while (psn >= 0 && strncmp(&str[psn], substr, sublen))
			  psn--;
			index = (int)psn;
		}
	}
	return index;
}
#endif

int ProjectManager::WriteFile(const char* name, const char* content) 
{
	int written = 0;

	FILE* fp = fopen(name, "wb");
	if (fp != NULL) {
		if (content != NULL)
		  written = (int)fwrite(content, 1, strlen(content), fp);
		fclose(fp);
	}
	return written;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
