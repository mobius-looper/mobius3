/**
 * This is a collection of interfaces for objects that provide
 * services to internal system components.
 *
 * It is conceptually similar to Provider but they have a more focused set
 * of interfaces that only do things relevant for a particular service.
 *
 * The initial example is the FileChooserService which is implemented by Prompter
 * and hides the implementation of both Prompter and Pathfinder.
 *
 * Need to consider breaking up Provider smaller pieces, some obvious ones would be:
 *
 *     MidiDeviceService
 *     AudioDeviceService
 *     RefreshService
 *     ConfigurationService
 *     FileService
 *
 */

#pragma once

class FileChooserService
{
  public:

    class Handler {
      public:
        virtual ~Handler() {}
        virtual void fileChooserResponse(juce::File f) = 0;
    };

    virtual ~FileChooserService() {}

    /**
     * Launch an asynchronous process to select a folder using an appropriate
     * file chooser UI.
     *
     * The *purpse* acts as an identifier to save and restore previous selections
     * made for this purpose so the user does not to havigate to the same location
     * every time a choice is needed.
     *
     * The *purpose* also services as identifier for the asynchronous request itself
     * and may be used with the cancelRequest() method to deregister the handler
     * callback pointer that was given to chooseFolder.
     *
     * Todo: may need to split the two usages of purpose into purpose and requestId.
     */
    virtual void fileChooserRequestFolder(juce::String purpose, Handler* handler) = 0;

    /**
     * Cancel a previous request made to this service.
     * Whether the visualization of the request is actually canceled or not isn't
     * guaranteed, but what is required is that the handler object passed in
     * a request is deregistered and will not be called when the asynchronous proces
     * eventually completes.
     */
    virtual void fileChooserCancel(juce::String purpose) = 0;

};
