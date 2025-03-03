/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Configuration related to OSC
 * This file defines only the configuration objects
 * The runtime handling of bindings and event processing is elsewhere
 * and to be ported.
 */

#include "../util/Util.h"

#include "Binding.h"
#include "OscConfig.h"

//////////////////////////////////////////////////////////////////////
//
// OscConfig
//
//////////////////////////////////////////////////////////////////////

OscConfig::OscConfig()
{
	init();
}

void OscConfig::init()
{
	mInputPort = 0;
	mOutputHost = nullptr;
	mOutputPort = 0;
	mBindings = nullptr;
    mWatchers = nullptr;
    // this is only for XML parsing, get rid of it
    mError[0] = 0;
}

OscConfig::~OscConfig()
{
	delete mOutputHost;
	delete mBindings;
    delete mWatchers;
}

const char* OscConfig::getError()
{
    return (mError[0] != 0) ? mError : nullptr;
}

int OscConfig::getInputPort()
{
	return mInputPort;
}

void OscConfig::setInputPort(int i)
{
	mInputPort = i;
}

const char* OscConfig::getOutputHost()
{
	return mOutputHost;
}

void OscConfig::setOutputHost(const char* s)
{
	delete mOutputHost;
	mOutputHost = CopyString(s);
}

int OscConfig::getOutputPort()
{
	return mOutputPort;
}

void OscConfig::setOutputPort(int i)
{
	mOutputPort = i;
}

OscBindingSet* OscConfig::getBindings()
{
	return mBindings;
}

void OscConfig::setBindings(OscBindingSet* list)
{
    delete mBindings;
    mBindings = list;
}

OscWatcher* OscConfig::getWatchers()
{
    return mWatchers;
}

void OscConfig::setWatchers(OscWatcher* list)
{
    delete mWatchers;
    mWatchers = list;
}

//////////////////////////////////////////////////////////////////////
//
// OscBindingSet
//
//////////////////////////////////////////////////////////////////////

OscBindingSet::OscBindingSet()
{
	init();
}

void OscBindingSet::init()
{
	mNext = nullptr;
    mName = nullptr;
    mComments = nullptr;
    mActive = false;
	mInputPort = 0;
	mOutputHost = nullptr;
	mOutputPort = 0;
	mBindings = nullptr;
}

OscBindingSet::~OscBindingSet()
{
	OscBindingSet *el, *next;

    delete mName;
    delete mComments;
	delete mOutputHost;
	delete mBindings;

	for (el = mNext ; el != nullptr ; el = next) {
		next = el->getNext();
		el->setNext(nullptr);
		delete el;
	}
}

OscBindingSet* OscBindingSet::getNext()
{
    return mNext;
}

void OscBindingSet::setNext(OscBindingSet* s)
{
    mNext = s;
}

const char* OscBindingSet::getName()
{
    return mName;
}

void OscBindingSet::setName(const char* s)
{
    delete mName;
    mName = CopyString(s);
}

const char* OscBindingSet::getComments()
{
    return mComments;
}

void OscBindingSet::setComments(const char* s)
{
    delete mComments;
    mComments = CopyString(s);
}

bool OscBindingSet::isActive()
{
    // ignore the active flag for now until we have a UI
    return true;
}

void OscBindingSet::setActive(bool b)
{
    mActive = b;
}

int OscBindingSet::getInputPort()
{
	return mInputPort;
}

void OscBindingSet::setInputPort(int i)
{
	mInputPort = i;
}

const char* OscBindingSet::getOutputHost()
{
	return mOutputHost;
}

void OscBindingSet::setOutputHost(const char* s)
{
	delete mOutputHost;
	mOutputHost = CopyString(s);
}

int OscBindingSet::getOutputPort()
{
	return mOutputPort;
}

void OscBindingSet::setOutputPort(int i)
{
	mOutputPort = i;
}

Binding* OscBindingSet::getBindings()
{
	return mBindings;
}

void OscBindingSet::setBindings(Binding* list)
{
    delete mBindings;
    mBindings = list;
}

//////////////////////////////////////////////////////////////////////
//
// OscWatcher
//
//////////////////////////////////////////////////////////////////////

OscWatcher::OscWatcher()
{
    init();
}

void OscWatcher::init()
{
    mNext = nullptr;
    mPath = nullptr;
    mName = nullptr;
    mTrack = 0;
}

OscWatcher::~OscWatcher()
{
    delete mName;
    delete mPath;
    
	OscWatcher *el, *next;
	for (el = mNext ; el != nullptr ; el = next) {
		next = el->getNext();
		el->setNext(nullptr);
		delete el;
	}
}

OscWatcher* OscWatcher::getNext()
{
    return mNext;
}

void OscWatcher::setNext(OscWatcher* w)
{
    mNext = w;
}

const char* OscWatcher::getPath()
{
    return mPath;
}

void OscWatcher::setPath(const char* path)
{
    delete mPath;
    mPath = CopyString(path);
}

const char* OscWatcher::getName()
{
    return mName;
}

void OscWatcher::setName(const char* name)
{
    delete mName;
    mName = CopyString(name);
}

int OscWatcher::getTrack()
{
    return mTrack;
}

void OscWatcher::setTrack(int t)
{
    mTrack = t;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
