// UserVariables lost built-in XML transformation in the external model
// which moved everything to XmlRenderer
//
// Projects used that, though UserVariables were rarely if ever used
// Projects are a mess in general, decide whether these should have
// a private XML transformer (probably) outside of XmlRenderer
// and restore UserVarables.toXml
//

/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A representation of the runtime state of a Mobius instance, 
 * including audio data.  This allows Mobius state to be saved to
 * and restored from files.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Mapper.h"

#include "../../util/Util.h"
#include "../../util/List.h"
#include "../../util/XmlModel.h"
#include "../../util/XmlBuffer.h"
#include "../../util/XomParser.h"

#include "../../model/UserVariable.h"

#include "../AudioPool.h"

#include "Expr.h"
#include "Loop.h"
#include "Mobius.h"
#include "Layer.h"
#include "Segment.h"
#include "Track.h"
#include "ParameterSource.h"

#include "Project.h"

/****************************************************************************
 *                                                                          *
 *   							XML CONSTANTS                               *
 *                                                                          *
 ****************************************************************************/

#define EL_PROJECT "Project"
#define EL_TRACK "Track"
#define EL_LOOP "Loop"
#define EL_LAYER "Layer"
#define EL_SEGMENT "Segment"

#define ATT_NUMBER "number"
#define ATT_BINDINGS "bindings"
#define ATT_MIDI_CONFIG "midiConfig"
#define ATT_SETUP "setup"
#define ATT_GROUP "group"
#define ATT_LAYER "layer"
#define ATT_OFFSET "offset"
#define ATT_START_FRAME "startFrame"
#define ATT_FRAMES "frames"
#define ATT_FEEDBACK "feedback"
#define ATT_COPY_LEFT "localCopyLeft"
#define ATT_COPY_RIGHT "localCopyRight"
 
#define ATT_ID "id"
#define ATT_CYCLES "cycles"
#define ATT_BUFFERS "buffers"
#define ATT_FRAME "frame"
#define ATT_REVERSE "reverse"
#define ATT_SPEED_OCTAVE "speedOctave"
#define ATT_SPEED_STEP "speedStep"
#define ATT_SPEED_BEND "speedBend"
#define ATT_SPEED_TOGGLE "speedToggle"
#define ATT_PITCH_OCTAVE "pitchOctave"
#define ATT_PITCH_STEP "pitchStep"
#define ATT_PITCH_BEND "pitchBend"
#define ATT_TIME_STRETCH "timeStretch"
#define ATT_OVERDUB "overdub"
#define ATT_ACTIVE "active"
#define ATT_AUDIO "audio"
#define ATT_PROTECTED "protected"
#define ATT_PRESET "preset"
#define ATT_FEEDBACK "feedback"
#define ATT_ALT_FEEDBACK "altFeedback"
#define ATT_INPUT "input"
#define ATT_OUTPUT "output"
#define ATT_PAN "pan"
#define ATT_FOCUS_LOCK "focusLock"
#define ATT_DEFERRED_FADE_LEFT "deferredFadeLeft"
#define ATT_DEFERRED_FADE_RIGHT "deferredFadeRight"
#define ATT_CONTAINS_DEFERRED_FADE_LEFT "containsDeferredFadeLeft"
#define ATT_CONTAINS_DEFERRED_FADE_RIGHT "containsDeferredFadeRight"
#define ATT_REVERSE_RECORD "reverseRecord"

#define EL_VARIABLES "Variables"

/****************************************************************************
 *                                                                          *
 *   						   PROJECT SEGMENT                              *
 *                                                                          *
 ****************************************************************************/

ProjectSegment::ProjectSegment()
{
	init();
}

ProjectSegment::ProjectSegment(Segment* src)
{
	init();

	mOffset = src->getOffset();
	mStartFrame = src->getStartFrame();
	mFrames = src->getFrames();
	mFeedback = src->getFeedback();
	mLocalCopyLeft = src->getLocalCopyLeft();
	mLocalCopyRight = src->getLocalCopyRight();

	Layer* l = src->getLayer();
	if (l != nullptr) {
		// !! need a more reliable id?
		mLayer = l->getNumber();
	}
}

ProjectSegment::ProjectSegment(XmlElement* e)
{
	init();
	parseXml(e);
}

void ProjectSegment::init()
{
	mOffset = 0;
	mStartFrame = 0;
	mFrames = 0;
	mLayer = 0;
	mFeedback = 127;
	mLocalCopyLeft = false;
	mLocalCopyRight = false;
}

Segment* ProjectSegment::allocSegment(Layer* layer)
{
	Segment* s = new Segment(layer);
	s->setOffset(mOffset);
	s->setStartFrame(mStartFrame);
	s->setFrames(mFrames);
	s->setFeedback(mFeedback);
	s->setLocalCopyLeft(mLocalCopyLeft);
	s->setLocalCopyRight(mLocalCopyRight);
	return s;
}

ProjectSegment::~ProjectSegment()
{
}

void ProjectSegment::setOffset(long f)
{
    mOffset = f;
}

long ProjectSegment::getOffset()
{
    return mOffset;
}

void ProjectSegment::setLayer(int id)
{
	mLayer = id;
}

int ProjectSegment::getLayer()
{
    return mLayer;
}

void ProjectSegment::setStartFrame(long f)
{
    mStartFrame = f;
}

long ProjectSegment::getStartFrame()
{
    return mStartFrame;
}

void ProjectSegment::setFrames(long l)
{
    mFrames = l;
}

long ProjectSegment::getFrames()
{
    return mFrames;
}

void ProjectSegment::setFeedback(int i)
{
    mFeedback = i;
}

int ProjectSegment::getFeedback()
{
    return mFeedback;
}

void ProjectSegment::setLocalCopyLeft(long frames)
{
	mLocalCopyLeft = frames;
}

long ProjectSegment::getLocalCopyLeft()
{
	return mLocalCopyLeft;
}

void ProjectSegment::setLocalCopyRight(long frames)
{
	mLocalCopyRight = frames;
}

long ProjectSegment::getLocalCopyRight()
{
	return mLocalCopyRight;
}

void ProjectSegment::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_SEGMENT);
    b->addAttribute(ATT_LAYER, mLayer);
    b->addAttribute(ATT_OFFSET, mOffset);
	b->addAttribute(ATT_START_FRAME, mStartFrame);
    b->addAttribute(ATT_FRAMES, mFrames);
    b->addAttribute(ATT_FEEDBACK, mFeedback);
    b->addAttribute(ATT_COPY_LEFT, mLocalCopyLeft);
    b->addAttribute(ATT_COPY_RIGHT, mLocalCopyRight);
	b->add("/>\n");
}

void ProjectSegment::parseXml(XmlElement* e)
{
	mLayer = e->getIntAttribute(ATT_LAYER);
	mOffset = e->getIntAttribute(ATT_OFFSET);
	mStartFrame = e->getIntAttribute(ATT_START_FRAME);
	mFrames = e->getIntAttribute(ATT_FRAMES);
	mFeedback = e->getIntAttribute(ATT_FEEDBACK);
	mLocalCopyLeft = e->getIntAttribute(ATT_COPY_LEFT);
	mLocalCopyRight = e->getIntAttribute(ATT_COPY_RIGHT);
}

/****************************************************************************
 *                                                                          *
 *                               PROJECT LAYER                              *
 *                                                                          *
 ****************************************************************************/

ProjectLayer::ProjectLayer()
{
	init();
}

ProjectLayer::ProjectLayer(XmlElement* e)
{
	init();
	parseXml(e);
}

ProjectLayer::ProjectLayer(Project* p, Layer* l)
{
    (void)p;
	init();

    // ids are only necessary if NoLayerFlattening is on and we
    // need to save LayerSegments, suppress if we're flattening to
    // avoid confusion 
    if (l->isNoFlattening())
      mId = l->getNumber();

	mCycles = l->getCycles();
	mDeferredFadeLeft = l->isDeferredFadeLeft();
	mDeferredFadeRight = l->isDeferredFadeRight();
	mContainsDeferredFadeLeft = l->isContainsDeferredFadeLeft();
	mContainsDeferredFadeRight = l->isContainsDeferredFadeRight();
	mReverseRecord = l->isReverseRecord();

    // if NoFlattening is on then we must save segments
    if (!l->isNoFlattening()) {

        // this will make a copy we own
        setAudio(l->flatten());

        // the Isolated Overdubs global parameter was experimental
        // and is no longer exposed, so this should never be true
        // and we won't have an mOverdub object or an mOverdubPath
		if (l->isIsolatedOverdub()) {
			Audio* a = l->getOverdub();
			if (a != nullptr && !a->isEmpty()) {
                // have to copy this since the mExternalAudio flag
                // must apply to both mAudio and mOverdub
                AudioPool* pool = a->getPool();
                if (pool == nullptr)
                  Trace(1, "ProjectLayer: no audio pool!\n");
                else {
                    Audio* ov = pool->newAudio();
                    ov->copy(a);
                    setOverdub(ov);
                    // since we're going to save this in a file, 
                    // set the correct sample rate
                    ov->setSampleRate(l->getLoop()->getMobius()->getSampleRate());
                }
            }
		}
    }
	else {
		// we don't own the Audio objects so don't delete them
		mExternalAudio = true;

		Audio* a = l->getAudio();
		if (!a->isEmpty())
		  setAudio(a);

		for (Segment* seg = l->getSegments() ; seg != nullptr ; 
			 seg = seg->getNext()) {
			ProjectSegment* ps = new ProjectSegment(seg);
			add(ps);
		}
	}  	
}

/**
 * Used when loading individual Audios from a file.
 */
ProjectLayer::ProjectLayer(Audio* a)
{
	init();
	setAudio(a);
}

void ProjectLayer::init()
{
	mId = 0;
	mCycles = 0;
	mSegments = nullptr;
    mAudio = nullptr;
	mOverdub = nullptr;
	mExternalAudio = false;
    mPath = nullptr;
	mOverdubPath = nullptr;
    mProtected = false;
	mDeferredFadeLeft = false;
	mDeferredFadeRight = false;
	mReverseRecord = false;
	mLayer = nullptr;
}

ProjectLayer::~ProjectLayer()
{
    delete mPath;
	if (!mExternalAudio) {
		delete mAudio;
		delete mOverdub;
	}
	if (mSegments != nullptr) {
		for (int i = 0 ; i < mSegments->size() ; i++) {
			ProjectSegment* s = (ProjectSegment*)mSegments->get(i);
			delete s;
		}
		delete mSegments;
	}

}

/**
 * Partially initialize a Layer object.
 * The segment list will be allocated later in resolveLayers.
 */
Layer* ProjectLayer::allocLayer(LayerPool* pool)
{
	if (mLayer == nullptr) {
		mLayer = pool->newLayer(nullptr);
		mLayer->setNumber(mId);

		if (mAudio != nullptr) {
			mLayer->setAudio(mAudio);
			mAudio = nullptr;
		}

        // this was an experimental feature that is no longer exposed
        // keep it around for awhile in case we want to resurect it
		if (mOverdub != nullptr) {
			mLayer->setOverdub(mOverdub);
			mLayer->setIsolatedOverdub(true);
			mOverdub = nullptr;
		}

        // when synthesizing Projects to load individual loops, not
        // all of the state may be filled out
        int cycles = mCycles;
        if (cycles <= 0) 
          cycles = 1;

		// !! need to restore the sync pulse count

		mLayer->setCycles(cycles);
		mLayer->setDeferredFadeLeft(mDeferredFadeLeft);
		mLayer->setContainsDeferredFadeLeft(mContainsDeferredFadeLeft);
		mLayer->setDeferredFadeRight(mDeferredFadeRight);
		mLayer->setContainsDeferredFadeRight(mContainsDeferredFadeRight);
		mLayer->setReverseRecord(mReverseRecord);
	}
	return mLayer;
}

void ProjectLayer::resolveLayers(Project* p)
{
	if (mLayer == nullptr) 
	  Trace(1, "Calling resolveLayers before layers allocated");

	else if (mSegments != nullptr) {
		for (int i = 0 ; i < mSegments->size() ; i++) {
			ProjectSegment* ps = (ProjectSegment*)mSegments->get(i);
			Layer* layer = p->findLayer(ps->getLayer());
			if (layer == nullptr) {
				Trace(1, "Unable to resolve project layer id %ld\n", 
					  (long)ps->getLayer());
			}
			else {
				Segment* s = ps->allocSegment(layer);
				mLayer->addSegment(s);
			}
		}
	}
}

int ProjectLayer::getId()
{
	return mId;
}

Layer* ProjectLayer::getLayer()
{
	return mLayer;
}

void ProjectLayer::setCycles(int i)
{
	mCycles = i;
}

int ProjectLayer::getCycles()
{
	return mCycles;
}

void ProjectLayer::setAudio(Audio* a)
{
	if (!mExternalAudio)
	  delete mAudio;
	mAudio = a;
}

Audio* ProjectLayer::getAudio()
{
	return mAudio;
}

Audio* ProjectLayer::stealAudio()
{
	Audio* a = mAudio;
	mAudio = nullptr;
	mExternalAudio = false;
	return a;
}

void ProjectLayer::setOverdub(Audio* a)
{
	if (!mExternalAudio)
	  delete mOverdub;
	mOverdub = a;
}

Audio* ProjectLayer::getOverdub()
{
	return mOverdub;
}

Audio* ProjectLayer::stealOverdub()
{
	Audio* a = mOverdub;
	mOverdub = nullptr;
	return a;
}

void ProjectLayer::setPath(const char* path)
{
	delete mPath;
    mPath = CopyString(path);
}

const char* ProjectLayer::getPath()
{
	return mPath;
}

void ProjectLayer::setOverdubPath(const char* path)
{
	delete mOverdubPath;
    mOverdubPath = CopyString(path);
}

const char* ProjectLayer::getOverdubPath()
{
	return mOverdubPath;
}

void ProjectLayer::setProtected(bool b)
{
	mProtected = b;
}

bool ProjectLayer::isProtected()
{
	return mProtected;
}

void ProjectLayer::setDeferredFadeLeft(bool b)
{
	mDeferredFadeLeft = b;
}

bool ProjectLayer::isDeferredFadeLeft()
{
	return mDeferredFadeLeft;
}

void ProjectLayer::setDeferredFadeRight(bool b)
{
	mDeferredFadeRight = b;
}

bool ProjectLayer::isDeferredFadeRight()
{
	return mDeferredFadeRight;
}

void ProjectLayer::setReverseRecord(bool b)
{
	mReverseRecord = b;
}

bool ProjectLayer::isReverseRecord()
{
	return mReverseRecord;
}

void ProjectLayer::add(ProjectSegment* seg)
{
	if (mSegments == nullptr)
	  mSegments = new List();
	mSegments->add(seg);
}

void ProjectLayer::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_LAYER);

    // this is required only if NoLayerFlattening is on and
    // we have to save LayerSegments, if we left it zero we
    // don't need it
    if (mId > 0)
      b->addAttribute(ATT_ID, mId);

	b->addAttribute(ATT_CYCLES, mCycles);
    b->addAttribute(ATT_AUDIO, mPath);
    b->addAttribute(ATT_OVERDUB, mOverdubPath);
    b->addAttribute(ATT_PROTECTED, mProtected);
    b->addAttribute(ATT_DEFERRED_FADE_LEFT, mDeferredFadeLeft);
    b->addAttribute(ATT_DEFERRED_FADE_RIGHT, mDeferredFadeRight);
    b->addAttribute(ATT_CONTAINS_DEFERRED_FADE_LEFT, mContainsDeferredFadeLeft);
    b->addAttribute(ATT_CONTAINS_DEFERRED_FADE_RIGHT, mContainsDeferredFadeRight);
    b->addAttribute(ATT_REVERSE_RECORD, mReverseRecord);

	if (mSegments == nullptr) 
	  b->add("/>\n");
	else {
		b->add(">\n");

		if (mSegments != nullptr) {
			b->incIndent();
			for (int i = 0 ; i < mSegments->size() ; i++) {
				ProjectSegment* seg = (ProjectSegment*)mSegments->get(i);
				seg->toXml(b);
			}
			b->decIndent();
		}

		b->addEndTag(EL_LAYER);
	}
}

void ProjectLayer::parseXml(XmlElement* e)
{
	mId = e->getIntAttribute(ATT_ID);	
	mCycles = e->getIntAttribute(ATT_CYCLES);
    mProtected = e->getBoolAttribute(ATT_PROTECTED);
    mDeferredFadeLeft = e->getBoolAttribute(ATT_DEFERRED_FADE_LEFT);
    mDeferredFadeRight = e->getBoolAttribute(ATT_DEFERRED_FADE_RIGHT);
    mContainsDeferredFadeLeft = e->getBoolAttribute(ATT_CONTAINS_DEFERRED_FADE_LEFT);
    mContainsDeferredFadeRight = e->getBoolAttribute(ATT_CONTAINS_DEFERRED_FADE_RIGHT);
    mReverseRecord = e->getBoolAttribute(ATT_REVERSE_RECORD);
	setPath(e->getAttribute(ATT_AUDIO));
	setOverdubPath(e->getAttribute(ATT_OVERDUB));

	for (XmlElement* child = e->getChildElement() ; child != nullptr ; 
		 child = child->getNextElement()) {

		add(new ProjectSegment(child));
	}
}

/****************************************************************************
 *                                                                          *
 *   							 PROJECT LOOP                               *
 *                                                                          *
 ****************************************************************************/

ProjectLoop::ProjectLoop()
{
	init();
}

ProjectLoop::ProjectLoop(XmlElement* e)
{
	init();
	parseXml(e);
}

ProjectLoop::ProjectLoop(Project* p, Loop* l)
{
	init();

    // hmm, capturing the current frame is bad for unit tests since 
    // KernelEvents will process the save event at a random time,
    // if it is ever useful to save this, will need a Project option
    // to prevent saving it in some cases
	//setFrame(l->getFrame());

	Layer* layer = l->getPlayLayer();
	while (layer != nullptr) {
		add(new ProjectLayer(p, layer));
		if (ParameterSource::isSaveLayers(l->getTrack()))
		  layer = layer->getPrev();
		else
		  layer = nullptr;
	}
}

void ProjectLoop::init()
{
    mNumber = 0;
	mLayers = nullptr;
	mFrame = 0;
	mActive = false;
}

ProjectLoop::~ProjectLoop()
{
	if (mLayers != nullptr) {
		for (int i = 0 ; i < mLayers->size() ; i++) {
			ProjectLayer* l = (ProjectLayer*)mLayers->get(i);
			delete l;
		}
		delete mLayers;
	}
}

void ProjectLoop::add(ProjectLayer* l)
{
	if (mLayers == nullptr)
	  mLayers = new List();
	mLayers->add(l);
}

void ProjectLoop::setNumber(int number)
{
	mNumber = number;
}

int ProjectLoop::getNumber()
{
	return mNumber;
}

List* ProjectLoop::getLayers()
{
	return mLayers;
}

void ProjectLoop::setFrame(long f)
{
	mFrame = f;
}

long ProjectLoop::getFrame()
{
	return mFrame;
}

void ProjectLoop::setActive(bool b)
{
	mActive = b;
}

bool ProjectLoop::isActive()
{
	return mActive;
}

/**
 * Helper for layer resolution at load time.
 */
Layer* ProjectLoop::findLayer(int id)
{
	Layer* found = nullptr;
	if (mLayers != nullptr) {
		for (int i = 0 ; i < mLayers->size() && found == nullptr ; i++) {
			ProjectLayer* l = (ProjectLayer*)mLayers->get(i);
			if (l->getId() == id)
			  found = l->getLayer();
		}
	}
	return found;
}

void ProjectLoop::allocLayers(LayerPool* pool)
{
	if (mLayers != nullptr) {
		for (int i = 0 ; i < mLayers->size() ; i++) {
			ProjectLayer* l = (ProjectLayer*)mLayers->get(i);
			l->allocLayer(pool);
		}
	}
}

void ProjectLoop::resolveLayers(Project* p)
{
	if (mLayers != nullptr) {
		for (int i = 0 ; i < mLayers->size() ; i++) {
			ProjectLayer* l = (ProjectLayer*)mLayers->get(i);
			l->resolveLayers(p);
		}
	}
}

void ProjectLoop::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_LOOP);
	b->addAttribute(ATT_ACTIVE, mActive);
	if (mFrame > 0)
	  b->addAttribute(ATT_FRAME, mFrame);

	if (mLayers == nullptr)
	  b->add("/>\n");
	else {
		b->add(">\n");

		if (mLayers != nullptr) {
			b->incIndent();
			for (int i = 0 ; i < mLayers->size() ; i++) {
				ProjectLayer* layer = (ProjectLayer*)mLayers->get(i);
				layer->toXml(b);
			}
			b->decIndent();
		}
		b->addEndTag(EL_LOOP);
	}
}

void ProjectLoop::parseXml(XmlElement* e)
{
	mActive = e->getBoolAttribute(ATT_ACTIVE);
	mFrame = e->getIntAttribute(ATT_FRAME);

	for (XmlElement* child = e->getChildElement() ; child != nullptr ; 
		 child = child->getNextElement()) {

		add(new ProjectLayer(child));
	}
}

/****************************************************************************
 *                                                                          *
 *   							PROJECT TRACK                               *
 *                                                                          *
 ****************************************************************************/

ProjectTrack::ProjectTrack()
{
	init();
}

ProjectTrack::ProjectTrack(XmlElement* e)
{
	init();
	parseXml(e);
}

ProjectTrack::ProjectTrack(Project* p, Track* t)
{
	int i;

	init();

    //mGroup = t->getGroup();
	//mFocusLock = t->isFocusLock();
	mInputLevel = t->getInputLevel();
	mOutputLevel = t->getOutputLevel();
	mFeedback = t->getFeedback();
	mAltFeedback = t->getAltFeedback();
	mPan = t->getPan();

    mSpeedOctave = t->getSpeedOctave();
    mSpeedStep = t->getSpeedStep();
    mSpeedBend = t->getSpeedBend();
    mSpeedToggle = t->getSpeedToggle();
    mPitchOctave = t->getPitchOctave();
    mPitchStep = t->getPitchStep();
    mPitchBend = t->getPitchBend();
    mTimeStretch = t->getTimeStretch();

	// suppress emitting XML for empty loops at the end
	int last = t->getLoopCount();
	for (i = last - 1 ; i >= 0 ; i--) {
		if (t->getLoop(i)->isEmpty())
		  last--;
	}

	for (i = 0 ; i < last ; i++) {
		Loop* l = t->getLoop(i);
		ProjectLoop* pl = new ProjectLoop(p, l);
		if (l == t->getLoop())
		  pl->setActive(true);
		add(pl);
	}
}

void ProjectTrack::init()
{
    mNumber = 0;
	mActive = false;
    mGroup = 0;
	mFocusLock = false;
	mInputLevel = 127;
	mOutputLevel = 127;
	mFeedback = 127;
	mAltFeedback = 127;
	mPan = 64;
    mSpeedOctave = 0;
    mSpeedStep = 0;
    mSpeedBend = 0;
    mSpeedToggle = 0;
    mPitchOctave = 0;
    mPitchStep = 0;
    mPitchBend = 0;
    mTimeStretch = 0;
	mLoops = nullptr;
	mVariables = nullptr;
}

ProjectTrack::~ProjectTrack()
{
	delete mVariables;

	if (mLoops != nullptr) {
		for (int i = 0 ; i < mLoops->size() ; i++) {
			ProjectLoop* l = (ProjectLoop*)mLoops->get(i);
			delete l;
		}
		delete mLoops;
	}
}

void ProjectTrack::setNumber(int number)
{
	mNumber = number;
}

int ProjectTrack::getNumber()
{
	return mNumber;
}

void ProjectTrack::setActive(bool b)
{
	mActive = b;
}

bool ProjectTrack::isActive()
{
	return mActive;
}

int ProjectTrack::getGroup()
{
    return mGroup;
}

void ProjectTrack::setGroup(int i)
{
    mGroup = i;
}

void ProjectTrack::setFeedback(int i)
{
	mFeedback = i;
}

int ProjectTrack::getFeedback()
{
	return mFeedback;
}

void ProjectTrack::setAltFeedback(int i)
{
	mAltFeedback = i;
}

int ProjectTrack::getAltFeedback()
{
	return mAltFeedback;
}

void ProjectTrack::setOutputLevel(int i)
{
	mOutputLevel = i;
}

int ProjectTrack::getOutputLevel()
{
	return mOutputLevel;
}

void ProjectTrack::setInputLevel(int i)
{
	mInputLevel = i;
}

int ProjectTrack::getInputLevel()
{
	return mInputLevel;
}

void ProjectTrack::setPan(int i)
{
	mPan = i;
}

int ProjectTrack::getPan()
{
	return mPan;
}

void ProjectTrack::setReverse(bool b)
{
	mReverse = b;
}

bool ProjectTrack::isReverse()
{
	return mReverse;
}

void ProjectTrack::setSpeedOctave(int i)
{
	mSpeedOctave = i;
}

int ProjectTrack::getSpeedOctave()
{
	return mSpeedOctave;
}

void ProjectTrack::setSpeedStep(int i)
{
	mSpeedStep = i;
}

int ProjectTrack::getSpeedStep()
{
	return mSpeedStep;
}

void ProjectTrack::setSpeedBend(int i)
{
	mSpeedBend = i;
}

int ProjectTrack::getSpeedBend()
{
	return mSpeedBend;
}

void ProjectTrack::setSpeedToggle(int i)
{
	mSpeedToggle = i;
}

int ProjectTrack::getSpeedToggle()
{
	return mSpeedToggle;
}

void ProjectTrack::setPitchOctave(int i)
{
	mPitchOctave = i;
}

int ProjectTrack::getPitchOctave()
{
	return mPitchOctave;
}

void ProjectTrack::setPitchStep(int i)
{
	mPitchStep = i;
}

int ProjectTrack::getPitchStep()
{
	return mPitchStep;
}

void ProjectTrack::setPitchBend(int i)
{
	mPitchBend = i;
}

int ProjectTrack::getPitchBend()
{
	return mPitchBend;
}

void ProjectTrack::setTimeStretch(int i)
{
	mTimeStretch = i;
}

int ProjectTrack::getTimeStretch()
{
	return mTimeStretch;
}

void ProjectTrack::setFocusLock(bool b)
{
	mFocusLock = b;
}

bool ProjectTrack::isFocusLock()
{
	return mFocusLock;
}

void ProjectTrack::add(ProjectLoop* l)
{
	if (mLoops == nullptr)
	  mLoops = new List();
	mLoops->add(l);
}

List* ProjectTrack::getLoops()
{
	return mLoops;
}

void ProjectTrack::setVariable(const char* name, ExValue* value)
{
	if (name != nullptr) {
		if (mVariables == nullptr)
		  mVariables = new UserVariables();
		mVariables->set(name, value);
	}
}

void ProjectTrack::getVariable(const char* name, ExValue* value)
{
	value->setNull();
	if (mVariables != nullptr)
	  mVariables->get(name, value);
}

Layer* ProjectTrack::findLayer(int id)
{
	Layer* found = nullptr;
	if (mLoops != nullptr) {
		for (int i = 0 ; i < mLoops->size() && found == nullptr ; i++) {
			ProjectLoop* l = (ProjectLoop*)mLoops->get(i);
			found = l->findLayer(id);
		}
	}
	return found;
}

void ProjectTrack::allocLayers(LayerPool* pool)
{
	if (mLoops != nullptr) {
		for (int i = 0 ; i < mLoops->size() ; i++) {
			ProjectLoop* l = (ProjectLoop*)mLoops->get(i);
			l->allocLayers(pool);
		}
	}
}

void ProjectTrack::resolveLayers(Project* p)
{
	if (mLoops != nullptr) {
		for (int i = 0 ; i < mLoops->size() ; i++) {
			ProjectLoop* l = (ProjectLoop*)mLoops->get(i);
			l->resolveLayers(p);
		}
	}
}

void ProjectTrack::toXml(XmlBuffer* b)
{
	toXml(b, false);
}

void ProjectTrack::toXml(XmlBuffer* b, bool isTemplate)
{
	b->addOpenStartTag(EL_TRACK);

    b->addAttribute(ATT_ACTIVE, mActive);

    if (mGroup > 0)
      b->addAttribute(ATT_GROUP, mGroup);
    b->addAttribute(ATT_FOCUS_LOCK, mFocusLock);

    b->addAttribute(ATT_INPUT, mInputLevel);
    b->addAttribute(ATT_OUTPUT, mOutputLevel);
    b->addAttribute(ATT_FEEDBACK, mFeedback);
    b->addAttribute(ATT_ALT_FEEDBACK, mAltFeedback);
    b->addAttribute(ATT_PAN, mPan);

    b->addAttribute(ATT_REVERSE, mReverse);
    b->addAttribute(ATT_SPEED_OCTAVE, mSpeedOctave);
    b->addAttribute(ATT_SPEED_STEP, mSpeedStep);
    b->addAttribute(ATT_SPEED_BEND, mSpeedBend);
    b->addAttribute(ATT_SPEED_TOGGLE, mSpeedToggle);
    b->addAttribute(ATT_PITCH_OCTAVE, mPitchOctave);
    b->addAttribute(ATT_PITCH_STEP, mPitchStep);
    b->addAttribute(ATT_PITCH_BEND, mPitchBend);
    b->addAttribute(ATT_TIME_STRETCH, mTimeStretch);


	if (mLoops == nullptr && mVariables == nullptr)
	  b->add("/>\n");
	else {
		b->add(">\n");
		b->incIndent();

		if (!isTemplate) {
			if (mLoops != nullptr) {
				for (int i = 0 ; i < mLoops->size() ; i++) {
					ProjectLoop* loop = (ProjectLoop*)mLoops->get(i);
					loop->toXml(b);
				}
			}
		}

        // UserVariables lost XML at some point, need to restore
#if 0        
		if (mVariables != nullptr)
		  mVariables->toXml(b);
#endif
        
		b->decIndent();
		b->addEndTag(EL_TRACK);
	}
}

void ProjectTrack::parseXml(XmlElement* e)
{
	setActive(e->getBoolAttribute(ATT_ACTIVE));
    setGroup(e->getIntAttribute(ATT_GROUP));
	setFocusLock(e->getBoolAttribute(ATT_FOCUS_LOCK));
	setInputLevel(e->getIntAttribute(ATT_INPUT));
	setOutputLevel(e->getIntAttribute(ATT_OUTPUT));
	setFeedback(e->getIntAttribute(ATT_FEEDBACK));
	setAltFeedback(e->getIntAttribute(ATT_ALT_FEEDBACK));
	setPan(e->getIntAttribute(ATT_PAN));

	for (XmlElement* child = e->getChildElement() ; child != nullptr ; 
		 child = child->getNextElement()) {

		if (child->isName(EL_VARIABLES)) {
			delete mVariables;
            // lost UserVariables XML
			//mVariables = new UserVariables(child);
            mVariables = nullptr;
		}
		else
		  add(new ProjectLoop(child));
	}
}

/****************************************************************************
 *                                                                          *
 *   							   PROJECT                                  *
 *                                                                          *
 ****************************************************************************/

Project::Project()
{
	init();
}

Project::Project(XmlElement* e)
{
	init();
	parseXml(e);
}

Project::Project(const char* file)
{
	init();
    setPath(file);
}

/**
 * Convenience method that builds the project hierarchy around
 * a single loop layer.  Used when you want to load .wav files
 * one at a time.
 *
 * Track and loop number are both relative to zero.
 */
Project::Project(Audio* a, int trackNumber, int loopNumber)
{
	init();

	ProjectTrack* track = new ProjectTrack();
	ProjectLoop* loop = new ProjectLoop();
    ProjectLayer* layer = new ProjectLayer(a);
    
    track->setNumber(trackNumber);
    loop->setNumber(loopNumber);

	loop->add(layer);
	track->add(loop);
	add(track);

    // this must be on
    mIncremental = true;
}

void Project::init()
{
	mNumber = 0;
	mPath = nullptr;
	mTracks = nullptr;
	mVariables = nullptr;
	mBindings = nullptr;
	mSetup = nullptr;

	mLayerIds = 0;
	mError = false;
	strcpy(mMessage, "");
	mIncremental = false;
    mIncludeAudio = true;

	//mFile = nullptr;
	//strcpy(mBuffer, "");
	//mTokens = nullptr;
	//mNumTokens = 0;
	//mFinished = false;
}

Project::~Project()
{
	clear();
}

void Project::clear()
{
	if (mTracks != nullptr) {
		for (int i = 0 ; i < mTracks->size() ; i++) {
			ProjectTrack* l = (ProjectTrack*)mTracks->get(i);
			delete l;
		}
		delete mTracks;
		mTracks = nullptr;
	}
	delete mVariables;
	mVariables = nullptr;
	delete mBindings;
	mBindings = nullptr;
	delete mSetup;
	mSetup = nullptr;
    delete mPath;
    mPath = nullptr;
}

void Project::setNumber(int i)
{
	mNumber = i;
}

int Project::getNumber()
{
	return mNumber;
}

int Project::getNextLayerId()
{
	return mLayerIds++;
}

Layer* Project::findLayer(int id)
{
	Layer* found = nullptr;
	if (mTracks != nullptr) {
		for (int i = 0 ; i < mTracks->size() && found == nullptr ; i++) {
			ProjectTrack* t = (ProjectTrack*)mTracks->get(i);
			found = t->findLayer(id);
		}
	}
	return found;
}

void Project::setBindings(const char* name)
{
	delete mBindings;
	mBindings = CopyString(name);
}

const char* Project::getBindings()
{
	return mBindings;
}

void Project::setSetup(const char* name)
{
	delete mSetup;
	mSetup = CopyString(name);
}

const char* Project::getSetup()
{
	return mSetup;
}

void Project::setVariable(const char* name, ExValue* value)
{
	if (name != nullptr) {
		if (mVariables == nullptr)
		  mVariables = new UserVariables();
		mVariables->set(name, value);
	}
}

void Project::getVariable(const char* name, ExValue* value)
{
	value->setNull();
	if (mVariables != nullptr)
	  mVariables->get(name, value);
}

void Project::setTracks(Mobius* m)
{
	int i;

	int last = m->getTrackCount();

	// suppress empty tracks at the end (unless they're using
	// a different preset
	// NO, these can different preset and other settings that 
	// are useful to preserve
	//for (i = last - 1 ; i >= 0 ; i--) {
	//if (m->getTrack(i)->isEmpty())
	//last--;
	//}

	for (i = 0 ; i < last ; i++) {
		Track* t = m->getTrack(i);
		ProjectTrack* pt = new ProjectTrack(this, t);
		if (t == m->getTrack())
		  pt->setActive(true);
		add(pt);
	}
}

void Project::setPath(const char* path)
{
	delete mPath;
	mPath = CopyString(path);
}

const char* Project::getPath()
{
	return mPath;
}

bool Project::isError()
{
	return mError;
}

const char* Project::getErrorMessage()
{
	return mMessage;
}

void Project::setErrorMessage(const char* msg)
{
	if (msg == nullptr)
	  strcpy(mMessage, "");
	else
	  strcpy(mMessage, msg);
	mError = true;
}

void Project::add(ProjectTrack* t)
{
	if (mTracks == nullptr)
	  mTracks = new List();
	mTracks->add(t);
}

List* Project::getTracks()
{
	return mTracks;
}

void Project::setIncremental(bool b)
{
	mIncremental = b;
}

bool Project::isIncremental()
{
	return mIncremental;
}

/**
 * Traverse the hierarchy to instantiate Layer and Segment objects and
 * resolve references between them.
 */
void Project::resolveLayers(LayerPool* pool)
{
	int i;
	if (mTracks != nullptr) {
		for (i = 0 ; i < mTracks->size() ; i++) {
			ProjectTrack* t = (ProjectTrack*)mTracks->get(i);
			t->allocLayers(pool);
		}
		for (i = 0 ; i < mTracks->size() ; i++) {
			ProjectTrack* t = (ProjectTrack*)mTracks->get(i);
			t->resolveLayers(this);
		}
	}
}

void Project::toXml(XmlBuffer* b)
{
	toXml(b, false);
}

void Project::toXml(XmlBuffer* b, bool isTemplate)
{
	b->addOpenStartTag(EL_PROJECT);
	b->addAttribute(ATT_NUMBER, mNumber);
	b->addAttribute(ATT_BINDINGS, mBindings);
	b->addAttribute(ATT_SETUP, mSetup);
	b->addAttribute(ATT_AUDIO, mPath);

	if (mTracks == nullptr && mVariables == nullptr)
	  b->add("/>\n");
	else {
		b->add(">\n");
		b->incIndent();

		if (mTracks != nullptr) {
			for (int i = 0 ; i < mTracks->size() ; i++) {
				ProjectTrack* track = (ProjectTrack*)mTracks->get(i);
				track->toXml(b, isTemplate);
			}
		}

        // lost UserVariables XML
#if 0        
		if (mVariables != nullptr)
		  mVariables->toXml(b);
#endif
        
		b->decIndent();
		b->addEndTag(EL_PROJECT);
	}
}

void Project::parseXml(XmlElement* e)
{
	setNumber(e->getIntAttribute(ATT_NUMBER));
	setPath(e->getAttribute(ATT_AUDIO));

    // recognize the old MidiConfig name, the MidiConfigs will
    // have been upgraded to BindingConfigs by now
    const char* bindings = e->getAttribute(ATT_BINDINGS);
    if (bindings == nullptr) 
      bindings = e->getAttribute(ATT_MIDI_CONFIG);
	setBindings(bindings);

	for (XmlElement* child = e->getChildElement() ; child != nullptr ; 
		 child = child->getNextElement()) {

		if (child->isName(EL_VARIABLES)) {
			delete mVariables;
            // we lost the ability for UserVariables to have XML at some point
            // should restore
			//mVariables = new UserVariables(child);
            mVariables = nullptr;
		}
		else
		  add(new ProjectTrack(child));
	}
}


/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
