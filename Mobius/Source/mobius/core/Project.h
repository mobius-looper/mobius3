/**
 * TODO: UserVariables lost an XML transform at some point, need to put that back.
 *
 * This is old but I'm trying not to make too many changes until it
 * is working again reliably.
 *
 * This file defines the Project model and includes the code that
 * walks over the internal objects like Track/Loop/Layer/Segment to
 * load and save them.
 *
 * File handling has been moved up a level to Projector.cpp
 *
 * Creating a project requires memory allocation so it must be done
 * above the audio thread while the engine is in a suspended state.
 * It could be done in the audio thread as long as the engine was suspended,
 * but even then, it can still take a significant amount of time, and if we're
 * a plugin that could impact the ability of the host to process other plugins.
 *
 * Ideally it would be done in the maintenance thread, since the UI thread is
 * also managed by the host and it is unclear what impact bogging that down would have.
 *
 * Loading a project is relatively fast.  It also requires memory allocation, but
 * most of it uses pooled objects.  It could be done while the engine is suspended
 * or within the audio thread like it used to.
 *
 * Constructing the Project hierarchy from the engine is mostly contained
 * in this file.  Going the other direction has a few helper methods in core
 * code like Track::loadProject and Loop::loadProject that pull things from the Project
 * model and build out the internal structure.  Moving that here would require making
 * more of the innards public which I don't like.
 *
 * It would be more consistent if Track/Loop/Layer/Segment had a symetrical pair
 * of interfaces, Track::saveProject to builds a ProjectTrack from internal state and
 * Track::loadProject to go the other direction.  But this does push depenencies on
 * a particular project model down into core code which is not as good when you want
 * to design a new model.  But you could also just write a model converter above all that
 * and let core deal with Project and levels above convert that to something else.
 *
 */

#ifndef PROJECT_H
#define PROJECT_H

/****************************************************************************
 *                                                                          *
 *   							   PROJECT                                  *
 *                                                                          *
 ****************************************************************************/

class ProjectSegment {

  public:

    ProjectSegment();
    ProjectSegment(class Segment* src);
    ProjectSegment(class XmlElement* e);
    ~ProjectSegment();

	void setOffset(long i);
	long getOffset();

	void setStartFrame(long i);
	long getStartFrame();

	void setFrames(long i);
	long getFrames();
	
	void setLayer(int id);
	int getLayer();

	void setFeedback(int i);
	int getFeedback();

	void toXml(class XmlBuffer* b);
	void parseXml(class XmlElement* e);

	void setLocalCopyLeft(long frames);
	long getLocalCopyLeft();
	
	void setLocalCopyRight(long frames);
	long getLocalCopyRight();

	class Segment* allocSegment(class Layer* layer);

  private:

    void init();

	long mOffset;
	long mStartFrame;
	long mFrames;
	int  mFeedback;
	int  mLayer;
	long mLocalCopyLeft;
	long mLocalCopyRight;

};

class ProjectLayer {

  public:

    ProjectLayer();
    ProjectLayer(class XmlElement* e);
    ProjectLayer(class Project* p, class Layer* src);
    ProjectLayer(class Audio* src);
    ~ProjectLayer();

	int getId();

	void setCycles(int i);
	int getCycles();

    void setAudio(class Audio* a);
    class Audio* getAudio();
    class Audio* stealAudio();
    void setOverdub(class Audio* a);
    class Audio* getOverdub();
    class Audio* stealOverdub();

	void setBuffers(int i);
	int getBuffers();

    void setPath(const char* path);
    const char* getPath();
    void setOverdubPath(const char* path);
    const char* getOverdubPath();

    void setProtected(bool b);
    bool isProtected();

	void add(ProjectSegment* seg);

	void toXml(class XmlBuffer* b);
	void parseXml(class XmlElement* e);

	void setDeferredFadeLeft(bool b);
	bool isDeferredFadeLeft();
	void setDeferredFadeRight(bool b);
	bool isDeferredFadeRight();
	void setReverseRecord(bool b);
	bool isReverseRecord();

	Layer* getLayer();
	Layer* allocLayer(class LayerPool* pool);
	void resolveLayers(Project* p);

  private:

    void init();

    /**
     * This is the unique layer number generated for debugging.
     * It is not currently included in the project XML because it's
     * hard to explain and not really necessary.
     */
    int mId;

	int mCycles;
	class List* mSegments;
    class Audio* mAudio;
	class Audio* mOverdub;
    char* mPath;
    char* mOverdubPath;
    bool mProtected;
	bool mDeferredFadeLeft;
	bool mDeferredFadeRight;
    bool mContainsDeferredFadeLeft;
    bool mContainsDeferredFadeRight;
	bool mReverseRecord;

	/**
	 * True if the mAudio and mOverdub objects are owneed by
	 * the active Layer rather than by the Project.  Should only
	 * be true when saving the active project.
	 */
	bool mExternalAudio;

	/**
	 * Transient, set during project loading.
	 * Segments can reference layers by id, and the layers can appear
	 * anywhere in the project hiearchy in any order.  To resolve references
	 * to layers, we'll first make a pass over the project allocating
	 * Layer objects for each ProjectLayer and attaching them here.
	 * Then we'll make another pass to flesh out the Segment lists
	 * resolving to these Layer objects.	
	 */
	Layer* mLayer;

};

class ProjectLoop {

  public:

	ProjectLoop();
	ProjectLoop(class XmlElement* e);
	ProjectLoop(class Project* proj, 
				class Loop* loop);
	~ProjectLoop();

	void add(ProjectLayer* a);
	class List* getLayers();
	Layer* findLayer(int id);
	void allocLayers(class LayerPool* pool);
	void resolveLayers(Project* p);

	void setFrame(long f);
	long getFrame();

    void setNumber(int i);
    int getNumber();

	void setActive(bool b);
	bool isActive();

	void toXml(class XmlBuffer* b);
	void parseXml(class XmlElement* e);

  private:

	void init();

    /**
     * Ordinal number of this loop from zero.
     * This is used only for incremental projects where each track
     * and loop must specify the target number.
     */
    int mNumber;

	/**
	 * A list of ProjectLayer objects representing the layers of this loop.
	 */
	class List* mLayers;

	/**
	 * The frame at the time of capture.
	 */
	long mFrame;

	/**
	 * True if this was the active loop at the time of capture.
	 */
	bool mActive;

    // TODO: If they're using "restore" transfer modes we should
    // save the speed and pitch state for each loop.

};

class ProjectTrack {

  public:

	ProjectTrack();
	ProjectTrack(class XmlElement* e);
	ProjectTrack(class Project* proj, 
				 class Track* track);
	~ProjectTrack();

    void setNumber(int n);
    int getNumber();

	void setActive(bool b);
	bool isActive();

    void setGroup(int i);
    int getGroup();

	void setFocusLock(bool b);
	bool isFocusLock();

	void setInputLevel(int i);
	int getInputLevel();

	void setOutputLevel(int i);
	int getOutputLevel();

	void setFeedback(int i);
	int getFeedback();

	void setAltFeedback(int i);
	int getAltFeedback();

	void setPan(int i);
	int getPan();

	void setReverse(bool b);
	bool isReverse();

    void setSpeedOctave(int i);
    int getSpeedOctave();

    void setSpeedStep(int i);
    int getSpeedStep();

    void setSpeedBend(int i);
    int getSpeedBend();

    void setSpeedToggle(int i);
    int getSpeedToggle();

    void setPitchOctave(int i);
    int getPitchOctave();

    void setPitchStep(int i);
    int getPitchStep();

    void setPitchBend(int i);
    int getPitchBend();

    void setTimeStretch(int i);
    int getTimeStretch();

	void add(ProjectLoop* l);
	class List* getLoops();

	void setVariable(const char* name, class ExValue* value);
	void getVariable(const char* name, class ExValue* value);

	Layer* findLayer(int id);
	void allocLayers(class LayerPool* pool);
	void resolveLayers(Project* p);

	void toXml(class XmlBuffer* b);
	void toXml(class XmlBuffer* b, bool isTemplate);
	void parseXml(class XmlElement* e);


  private:

	void init();

    /**
     * Ordinal number of this loop from zero.
     * This is used only for incremental projects where each track
     * and loop must specify the target number.
     */
    int mNumber;

	// state at the time of the project snapshot, may be different
	// than the state in the Setup
	bool mActive;
	bool mFocusLock;
    int mGroup;
	int mInputLevel;
	int mOutputLevel;
	int mFeedback;
	int mAltFeedback;
	int mPan;

	bool mReverse;
    int mSpeedOctave;
    int mSpeedStep;
    int mSpeedBend;
    int mSpeedToggle;
    int mPitchOctave;
    int mPitchStep;
    int mPitchBend;
    int mTimeStretch;

	/**
	 * A list of ProjectLoop objects representing the loops
	 * in this track.  The length of the list is not necessarily the
	 * same as the MoreLoops parameter in the Mobius you are loading
	 * it into.  If it is less, empty loops are added, if it is more,
	 * MoreLoops is increased.
	 */
	class List* mLoops;
	
	/**
	 * User defined variable saved with the track.
	 */
	class UserVariables* mVariables;

};

/**
 * An object representing a snapshot of Mobius audio data and other
 * settings.  This may be as simple as a single .wav file for
 * the current loop, or as complicated as 8 tracks of 8 loops
 * with unlimited undo layers.
 *
 * NOTE: There are many relatively unusual things that are not 
 * saved in the project such as input and output port overrides.
 * Potentially everything that is in the Setup needs to be
 * in the ProjectTrack since it may be overridden.
 *
 * There are also lots of loop modes that aren't being saved
 * such as rate and pitch shift, mute mode, etc.  
 */
class Project {

  public:

	Project();
	Project(class XmlElement* e);
	Project(const char* file);
	Project(class Audio* a, int trackNumber, int loopNumber);
	~Project();

	void setNumber(int i);
	int getNumber();
	
    void setPath(const char* path);
    const char* getPath();

	//
	// Audio hierarchy specification
	//

	void clear();
	Layer* findLayer(int id);
	void resolveLayers(class LayerPool* pool);
	void setTracks(class Mobius* m);
	void add(ProjectTrack* t);
	class List* getTracks();

	void setVariable(const char* name, class ExValue* value);
	void getVariable(const char* name, class ExValue* value);

	void setBindings(const char* name);
	const char* getBindings();

	void setSetup(const char* name);
	const char* getSetup();

	//
	// Load options
	//

	void setIncremental(bool b);
	bool isIncremental();

	//
	// Save options
	//

	//void setIncludeUndoLayers(bool b);
	//bool isIncludeUndoLayers();

	//
	// Transient save/load state
	//

	bool isError();
	const char* getErrorMessage();
	void setErrorMessage(const char* msg);
	int getNextLayerId();
	
	//bool isFinished();
	//void setFinished(bool b);

	void toXml(class XmlBuffer* b);
	void toXml(class XmlBuffer* b, bool isTemplate);
	void parseXml(class XmlElement* e);

  private:

	void init();

	//
	// Persistent fields
	//

	/**
	 * Projects that can be referenced as VST parameters must
	 * have a unique number.
     * !! Huh?  I don't think this ever worked, you can't ref
     * projects as VST parameters, really?
	 */
	int mNumber;

    /**
     * The file we were loaded from or will save to.
     */
    char* mPath;

	/**
	 * A list of ProjectTrack objects.
	 */
	class List* mTracks;

	/**
	 * User defined global variables.
     * Might want these to move these to the Setup...
	 */
	class UserVariables* mVariables;

	/**
	 * Currently selected binding overlay.
	 */
	char* mBindings;

	/**
	 * Currently selected track setup.	
	 */
	char* mSetup;

	//
	// Runtime fields
	//

	/**
	 * Used to generate unique layer ids for segment references.
	 */
	int mLayerIds;

	/**
	 * Set during read() if an error was encountered.
	 */
	bool mError;

	/**
	 * Set during read() if an error was encountered.
	 */
	char mMessage[1024 * 2];

	/**
	 * When true, the project is incrementally merged with existing
	 * tracks rather than resetting all tracks first.
	 */
	bool mIncremental;

    /**
     * When true, layer Audio will loaded with the project.
     * WHen false, only the path name to the layer Audio file
     * is loaded.
     */
    bool mIncludeAudio;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif

