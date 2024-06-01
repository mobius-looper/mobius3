/**
 * File utilities using Juce
 */

#include <JuceHeader.h>
#include "Util.h"

/**
 * Read a file entirely into a string.
 * The returned string must be deleted.
 * Should start using String or std::string
 * but until everything gets converted here we are.
 */

char* ReadFile(const char* path)
{
    char* string = nullptr;

    juce::File file (path);
    if (!file.existsAsFile()) {
        DBG("File does not exist");
    }
    else {
        juce::FileInputStream input (file);
        if (!input.openedOk()) {
            DBG("Problem opening FileInputStream");
        }
        else {
            juce::String content = input.readString();
            string = CopyString(static_cast<const char*> (content.toUTF8()));
        }
    }

    return string;
}

/**
 * Write a file from a string.
 * Overwrite it if it already exists.
 * Return non-zero if there was an error.
 */
// this doesn't work, output stream fails
bool WriteFileWithTemp(const char* path, const char* contents)
{
    bool success = false;
    
    // example shows writing to a temp file, I guess to prevent
    // trashing an existing file if something goes wrong
    juce::TemporaryFile tempFile (path);
    juce::FileOutputStream output (tempFile.getFile());

    if (! output.openedOk()) {
        DBG ("FileOutputStream didn't open correctly ...");
    }
    else {
        output.setNewLineString("\n");
        output.writeString(contents);
        output.flush(); // (called explicitly to force an fsync on posix)

        if (output.getStatus().failed()) {
            DBG ("An error occurred in the FileOutputStream");
        }
        else {
            success = tempFile.overwriteTargetFileWithTemporary();    
        }
    }

    return success;
}

bool WriteFile(const char* path, const char* contents)
{
    bool success = false;

    juce::File file (path);

    // this was Jules' suggestion in 2018
    if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream())) {
        stream->setPosition(0);
        stream->truncate();

        // this works but it leaves weird zero bytes at the end, wtf?
        //stream->writeString("\n");
        stream->write(contents, strlen(contents));
        
        stream->flush(); // (called explicitly to force an fsync on posix)

        if (stream->getStatus().failed()) {
            DBG ("An error occurred in the FileOutputStream");
        }
        else {
            success = true;
        }

    }
    else {
        DBG("Unable to create FileOutputStream");
    }

    return success;
}
