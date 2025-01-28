/**
 * Subcomponent of Producer that deals with Session files.
 */

#pragma once

#include <JuceHeader.h>

#include "model/Session.h"

class SessionClerk
{
  public:

    class Folder {
      public:

        /**
         * The user visible leaf folder name
         */
        juce::String name;

        /**
         * The full path to the folder
         */
        juce::String path;

        /**
         * True if the folder exists and has been verified
         */
        bool valid = false;
    };

    SessionClerk(class Provider* p);
    ~SessionClerk();

    void initialize();

    class Session* readDefaultSession();
    Session* readSession(juce::String name);
    void saveSession(Session* s, juce::StringArray& errors);

    juce::OwnedArray<Folder>* getFolders();
    
  private:

    class Provider* provider = nullptr;
    juce::OwnedArray<Folder> folders;
    juce::File libraryRoot;
    bool libraryValid = false;

    bool bootstrapDefaultSession();
    Folder* findFolder(juce::String name);
    
    juce::XmlElement* readSessionElement(juce::File src);
    void logErrors(const char* filename, juce::StringArray& errors);
    
    Session* readSession(Folder* f);
    void writeSession(Folder* f, Session* s, juce::StringArray& errors);

    void migrateSetups(bool bootstrapped);

    void createSession(Session* neu, juce::StringArray& errors);
    
    Folder* createFolder(juce::String name, juce::StringArray& errors);

    void fixSession(class Session* s);
    void renameSession(juce::String name, juce::String newName, juce::StringArray& errors);
    void addError(juce::StringArray& errors, juce::String msg);
};
