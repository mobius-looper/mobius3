/**
 * Utility to read and write project files.
 * This used to be intertwined with core/Project.cpp but split
 * out so we can keep file handling above the kernel.
 */

#include <JuceUtil.h>

Projector::Projector()
{
}

Projector::~Projector()
{
}

/****************************************************************************
 *                                                                          *
 *   							   FILE IO                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Read the project structure but no audio files.
 */
void Project::read()
{
    if (mPath != NULL)
      read(NULL, mPath);
}

void Project::read(AudioPool* pool)
{
    if (mPath != NULL)
      read(pool, mPath);
}

void Project::read(AudioPool* pool, const char* file)
{
	char path[1024];

	mError = false;
	strcpy(mMessage, "");

	if (strchr(file, '.') != NULL)
	  strcpy(path, file);
	else {
		// auto extend
		snprintf(path, sizeof(path), "%s.mob", file);
	}

	FILE* fp = fopen(path, "r");
	if (fp == NULL) {
		snprintf(mMessage, sizeof(mMessage), "Unable to open file %s\n", path);
		mError = true;
	}
	else {
		fclose(fp);
        
        XomParser* p = new XomParser();
        XmlDocument* d = p->parseFile(path);
        if (d != NULL) {
            XmlElement* e = d->getChildElement();
            if (e != NULL) {
                clear();
                parseXml(e);
            }
            delete d;
        }
        else {
            // there was a syntax error in the file
            snprintf(mMessage, sizeof(mMessage), "Unable to read file %s: %s\n", 
                    path, p->getError());
            mError = true;
        }
        delete p;

        readAudio(pool);
    }
}

/**
 * After reading the Project structure from XML, traverse the hierarhcy
 * and load any referenced Audio files.
 */
// !! FILES
void Project::readAudio(AudioPool* pool)
{
    (void)pool;
#if 0    
    if (pool != NULL && mTracks != NULL) {
        for (int i = 0 ; i < mTracks->size() ; i++) {
            ProjectTrack* track = (ProjectTrack*)mTracks->get(i);
            List* loops = track->getLoops();
            if (loops != NULL) {
                for (int j = 0 ; j < loops->size() ; j++) {
                    ProjectLoop* loop = (ProjectLoop*)loops->get(j);
                    List* layers = loop->getLayers();
                    if (layers != NULL) {
                        for (int k = 0 ; k < layers->size() ; k++) {
                            ProjectLayer* layer = (ProjectLayer*)layers->get(k);
                            const char* path = layer->getPath();
                            if (path != NULL)
							  layer->setAudio(pool->newAudio(path));
                            path = layer->getOverdubPath();
                            if (path != NULL)
							  layer->setOverdub(pool->newAudio(path));
                        }
                    }
                }
            }
        }
    }
#endif    
}

void Project::write()
{
    if (mPath != NULL)
      write(mPath, false);
}

void Project::write(const char* file, bool isTemplate)
{
	char path[1024];
	char baseName[1024];

	mError = false;
	strcpy(mMessage, "");

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
        Project* existing = new Project(path);
        existing->deleteAudioFiles();
        delete existing;
    }

	// write the new project and Audio files
	fp = fopen(path, "w");
	if (fp == NULL) {
		snprintf(mMessage, sizeof(mMessage), "Unable to open output file: %s\n", path);
		mError = true;
	}
	else {
		fclose(fp);

		// first write Audio files and assign Layer paths
        if (!isTemplate)
          writeAudio(baseName);
			
        // then write the XML directory
        XmlBuffer* b = new XmlBuffer();
        toXml(b, isTemplate);
        WriteFileStub(path, b->getString());
        delete b;
	}
}

void Project::writeAudio(const char* baseName)
{
	if (mTracks != NULL) {
		for (int i = 0 ; i < mTracks->size() ; i++) {
			ProjectTrack* track = (ProjectTrack*)mTracks->get(i);
			track->writeAudio(baseName, i + 1);
		}
	}
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
 */
void Project::deleteAudioFiles()
{
    if (mTracks != NULL) {
        for (int i = 0 ; i < mTracks->size() ; i++) {
            ProjectTrack* track = (ProjectTrack*)mTracks->get(i);
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

void ProjectLayer::writeAudio(const char* baseName, int tracknum, int loopnum,
							  int layernum)
{
	char path[1024];

    if (mAudio != NULL && !mAudio->isEmpty() && !mProtected) {

        // todo: need to support inline audio in the XML
        snprintf(path, sizeof(path), "%s-%d-%d-%d.wav", baseName, 
				tracknum, loopnum, layernum);

        // Remember the new path too, should we every try to reuse
        // the previous path?  could be out of order by now
        setPath(path);

        // like memory allocation, Project is going to need to write files
        // push it up to Container? or should we just allow limited
        // file access down here?
        // will probably want Container to have more control over paths
        WriteAudio(mAudio, path);
    }

	if (mOverdub != NULL && !mOverdub->isEmpty()) {
        // todo: need to support inline audio in the XML
        snprintf(path, sizeof(path), "%s-%d-%d-%d-overdub.wav", baseName, 
				tracknum, loopnum, layernum);
		setOverdubPath(path);
		WriteAudio(mOverdub, path);
	}

}

void Project::setFinished(bool b)
{
	mFinished = b;
}

bool Project::isFinished()
{
	return mFinished;
}


void ProjectLoop::writeAudio(const char* baseName, int tracknum, int loopnum)
{
	if (mLayers != NULL) {
		for (int i = 0 ; i < mLayers->size() ; i++) {
			ProjectLayer* layer = (ProjectLayer*)mLayers->get(i);
			// use the layer id, it makes more sense
			//int layernum = i + 1;
			int layernum = layer->getId();
			layer->writeAudio(baseName, tracknum, loopnum, layernum);
		}
	}
}

void ProjectTrack::writeAudio(const char* baseName, int tracknum)
{
	if (mLoops != NULL) {
		for (int i = 0 ; i < mLoops->size() ; i++) {
			ProjectLoop* loop = (ProjectLoop*)mLoops->get(i);
			loop->writeAudio(baseName, tracknum, i + 1);
		}
	}
}

