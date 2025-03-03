/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for Mobius OSC configuration.
 *
 * I removed a bundh of runtime classes, will need another file
 * for those when the time comes.
 */

#ifndef OSC_CONFIG_H
#define OSC_CONFIG_H

//////////////////////////////////////////////////////////////////////
//
// OscConfig
//
//////////////////////////////////////////////////////////////////////

/**
 * An object containing all OSC configuration.  There is only
 * one of these, maintained with in the MobiusConfig.  
 */
class OscConfig {
  public:

	OscConfig();
	~OscConfig();

    // what's this for, parsing?
    // yes, think of a better way to handle this
    const char* getError();

	int getInputPort();
	void setInputPort(int i);
	const char* getOutputHost();
	void setOutputHost(const char* s);
	int getOutputPort();
	void setOutputPort(int i);

	class OscBindingSet* getBindings();
    void setBindings(class OscBindingSet* list);
    
    class OscWatcher* getWatchers();
    void setWatchers(class OscWatcher* list);

  private:

    void init();

	/**
	 * The default port on which we listen for OSC messages.
	 * Each OscBindingSet can specify a different input port in 
	 * case you want different mappings for more than one of the 
	 * same device.	
	 */
	int mInputPort;

	/**
	 * The default host to which we send OSC messages.
	 * Each OscBindingSet can specify a different output host in 
	 * case you have more than one device that needs to be updated.
	 */
	char* mOutputHost;

	/**
	 * The default port to which we send OSC messages.
	 * This must be set if mOutputHost is set, there is no default.
	 */
	int mOutputPort;

	/**
	 * Binding sets.  Unlike BindinConfigs several of these can be
	 * active at a time.
	 */
	class OscBindingSet* mBindings;

    /**
     * Exports.  The definitions of things that may be exported
     * from Mobius but which aren't controls or parameters and
     * can't be bound.
     */
    class OscWatcher* mWatchers;
    
    // parser error
    char mError[256];
};

//////////////////////////////////////////////////////////////////////
//
// OscBindingSet
//
//////////////////////////////////////////////////////////////////////

/**
 * A named collection of OSC bindings.
 * These don't extend Bindable because you can't activate them in the
 * same way as BindingConfigs.  No script access at the moment, I guess
 * we would need a global variable containing a CSV of the active set names.
 */
class OscBindingSet {
  public:

	OscBindingSet();
	~OscBindingSet();

	void setNext(OscBindingSet* s);
	OscBindingSet* getNext();

    void setName(const char* s);
    const char* getName();

    void setComments(const char* s);
    const char* getComments();

    void setActive(bool b);
    bool isActive();

	int getInputPort();
	void setInputPort(int i);
	const char* getOutputHost();
	void setOutputHost(const char* s);
	int getOutputPort();
	void setOutputPort(int i);

	class Binding* getBindings();
	void setBindings(class Binding* b);
	void addBinding(class Binding* c);
	void removeBinding(class Binding* c);

  private:

    void init();

	/**
	 * Chain link.
	 */
	OscBindingSet* mNext;

    /**
     * Sets should have names to distinguish them.
     */
    char* mName;

    /**
     * Optional comments describing the incomming messages that
     * may be bound.
     */
    char* mComments;

    /**
     * True if this is to be active. 
     * Ignored now, maybe this should be true to disable?
     */
    bool mActive;

	/**
	 * The port on which we listen for OSC messages.  This overrides
	 * the default port in the OscConfig.  This is relatively unusual but
	 * would be used if you want different mappings for more than one of the
	 * same device (e.g. two TouchOSC's controlling different sets of tracks.
	 */
	int mInputPort;

	/**
	 * The host to which we send OSC messages.
	 * This overrides the default host in the OscConfig.  You would override
	 * this if there is more than one device that needs status messages.
	 */
	char* mOutputHost;

	/**
	 * The default port to which we send OSC messages.
	 * This must be set if mOutputHost is set, there is no default.
	 */
	int mOutputPort;

	/**
	 * Bindings for this set.
	 * You can mix bindings from different devices but if you
	 * want to use bidirectional feedback you should only put
	 * bindings for one device in a set, because the set can have
	 * only one outputHost/outputPort.
	 */
	class Binding* mBindings;

};

//////////////////////////////////////////////////////////////////////
//
// OscWatcher
//
////////////////////////////////////////////////////////////////////////

class OscWatcher {

  public:

    OscWatcher();
    ~OscWatcher();

    OscWatcher* getNext();
    void setNext(OscWatcher* e);

    const char* getPath();
    void setPath(const char* path);

    const char* getName();
    void setName(const char* name);

    int getTrack();
    void setTrack(int t);

  private:

    void init();

    OscWatcher* mNext;
    char* mPath;
    char* mName;
    int mTrack;
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
