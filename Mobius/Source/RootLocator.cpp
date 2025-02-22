/**
 * Simple utility to locate the root directory where configuration
 * files are expected to be stored.  If a path in a configuration
 * object is relative, use the derived root directory to calculate
 * the full paths to things.
 *
 * If this were a formal installation, the installer was expected
 * to create directories under an OS-specific "application data" folder.
 *
 * On Windows this is:
 *
 *     c:/Users/<username>/AppData/Local
 *
 * On Mac this is:
 *
 *     /Users/<username>/Library
 *
 * Under the appdata folder we look for "Circular Labs/Mobius"
 *
 * For developer convenience without a formal install, we will bootstrap
 * the configuration directory if we can, and place a mobius-redirect file
 * in it with the path to the development directory.
 *
 * On failure, the root defaults to the user's home directory, and it won't be pretty.
 *
 * The root will normally be a standard configuration directory, but with the
 * addition of a single redirect file, the Mobius configuration files, mobius.xml
 * ui.xml, ScriptConfig paths, SampleConfig paths, etc.  can be redirected elsewhere,
 * typically a folder with a special configuration designed for unit tests without
 * distrupting the configuration for normal use.
 *
 * Typical output of whereAmI()
 *
 * Windows
 * 
 * Current working directory: C:\dev\jucetest\UI\Builds\VisualStudio2022
 * Current executable file: C:\dev\jucetest\UI\Builds\VisualStudio2022\x64\Debug\App\UI.exe
 * Current application file: C:\dev\jucetest\UI\Builds\VisualStudio2022\x64\Debug\App\UI.exe
 * Invoked executable file: C:\dev\jucetest\UI\Builds\VisualStudio2022\x64\Debug\App\UI.exe
 * Host application path: C:\dev\jucetest\UI\Builds\VisualStudio2022\x64\Debug\App\UI.exe
 * User home directory: C:\Users\Jeff
 * User application data directory: C:\Users\Jeff\AppData\Roaming
 * Common application data directory: C:\ProgramData
 * Common documents directory: C:\Users\Public\Documents
 * Temp directory: c:\temp
 * Windows system directory: C:\WINDOWS\system32
 * Global applications directory: C:\Program Files
 * Windows local app data directory: C:\Users\Jeff\AppData\Local
 *
 * Mac
 * 
 * Current working directory: /
 * Current executable file: /Users/jeff/dev/jucetest/UI/Builds/MacOSX/build/Debug/UI.app/Contents/MacOS/UI
 * Current application file: /Users/jeff/dev/jucetest/UI/Builds/MacOSX/build/Debug/UI.app
 * Invoked executable file: /Users/jeff/dev/jucetest/UI/Builds/MacOSX/build/Debug/UI.app/Contents/MacOS/UI
 * Host application path: /Users/jeff/dev/jucetest/UI/Builds/MacOSX/build/Debug/UI.app/Contents/MacOS/UI
 * User home directory: /Users/jeff
 * User application data directory: /Users/jeff/Library
 * Common application data directory: /Library
 * Common documents directory: /Users/Shared
 * Temp directory: /Users/jeff/Library/Caches/UI util/
 *
 */

#include <JuceHeader.h>

// don't like this dependency but VStudio makes printf useless
#include "util/Trace.h"

#include "RootLocator.h"

//////////////////////////////////////////////////////////////////////
//
// Static Interface
//
// Used in environments that need a quick answer and don't want to mess
// with maiintaining a singleton object.
//
//////////////////////////////////////////////////////////////////////

/**
 * Don't normally like macros, but it's unclear what the lifecycle
 * is on the Strings and their UTF8 representation.
 * (const char*) cast to avoid a warning about Juce returning
 * a juce::CharPointer_UTF8
 */
#define PATHSTRING(file) (const char*)(file.getFullPathName().toUTF8())

#define CONSTSTRING(jstring) (const char*)(jstring.toUTF8())

/**
 * Early diagnostic tool to show the various special directories
 * Juce is able to locate.
 */
void RootLocator::whereAmI()
{
    juce::File f;

    f = juce::File::getCurrentWorkingDirectory();
    trace("Current working directory: %s\n", PATHSTRING(f));

    // this is almost always useless, on Mac it is inside the package folder
    // on Windows it would be Program Files if running an installation and
    // under the Juce Builds directory if running VS with a Juce Project
    f = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    trace("Current executable file: %s\n", f.getFullPathName().toUTF8());

    // same issues as ExecutableFile
    f = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    trace("Current application file: %s\n", f.getFullPathName().toUTF8());

    f = juce::File::getSpecialLocation(juce::File::invokedExecutableFile);
    trace("Invoked executable file: %s\n", f.getFullPathName().toUTF8());

    // might be useful for plugins
    f = juce::File::getSpecialLocation(juce::File::hostApplicationPath);
    trace("Host application path: %s\n", f.getFullPathName().toUTF8());

    f = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    trace("User home directory: %s\n", f.getFullPathName().toUTF8());

    // The "application data" directories are where things should be stored

    // docs say on Windows often \Documents and Settings\...
    // looks old, now says c:\Users\Jeff\AppData
    // on Mac docs say ~/Library
    f = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    trace("User application data directory: %s\n", f.getFullPathName().toUTF8());

    // on Mac probably /Library
    f = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);
    trace("Common application data directory: %s\n", f.getFullPathName().toUTF8());

    f = juce::File::getSpecialLocation(juce::File::commonDocumentsDirectory);
    trace("Common documents directory: %s\n", f.getFullPathName().toUTF8());

    f = juce::File::getSpecialLocation(juce::File::tempDirectory);
    trace("Temp directory: %s\n", f.getFullPathName().toUTF8());

#ifndef __APPLE__
    f = juce::File::getSpecialLocation(juce::File::windowsSystemDirectory);
    trace("Windows system directory: %s\n", f.getFullPathName().toUTF8());

    f = juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory);
    trace("Global applications directory: %s\n", f.getFullPathName().toUTF8());

    f = juce::File::getSpecialLocation(juce::File::windowsLocalAppData);
    trace("Windows local app data directory: %s\n", f.getFullPathName().toUTF8());
#endif
    
}

/**
 * Figure out where to get things.
 *
 * checkRedirect will just walk a redirect chain without looking
 * for specific folder content.
 *
 * todo: Is it worth messing with environment variables or the Windows registry?
 * OG Mobius had the installer leave registry entries behind so we could remember
 * the user selections if they deviated from the norm.
 *
 * This one is static so it can be more easily used in random places.
 * Errors are returned in the supplied array.
 *
 */
juce::File RootLocator::getRoot(juce::StringArray& errors)
{
    juce::File verifiedRoot;
    char error[1024];
    errors.clear();

    const char* companyName = "Circular Labs";
    const char* productName = "Mobius";
        
#ifdef __APPLE__
    // this is normally /Users/user/Library
    // if you ask for commonApplicationDataDirectory it will be /Library
    // I had permission problems dumping things in /Library in the past let's force
    // it under the user for now, which is probably what most people want anyway
    juce::File appdata = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#else
    // this is normally c:\Users\user\AppData\Local
    // note that if you use userApplicationDataDirectory like is done on Mac this ended
    // up under "Roaming" rather than "Local" and don't want to fuck with that shit
    // there are then three places (at least) this could go: User/AppData/Local, User/AppData/Roaming
    // and c:\ProgramData for "all users" which is commonApplicationDataDirectory
    juce::File appdata = juce::File::getSpecialLocation(juce::File::windowsLocalAppData);
#endif        

    juce::File mobiusinst;
        
    if (!appdata.isDirectory()) {
        // something is seriously different about this machine, bail
        snprintf(error, sizeof(error), "Normal root location does not exist: %s\n",
                 PATHSTRING(appdata));
        errors.add(juce::String(error));
    }
    else {
        trace("RootLocator: Starting root exploration in: %s\n", PATHSTRING(appdata));
#ifdef __APPLE__
        // this is normally ~/Library
        // the convention seems to be that products put their stuff under
        // Application Support, then company/product folders
        const char* appSupportName = "Application Support";
        juce::File appsupport = appdata.getChildFile(appSupportName);
        if (appsupport.isDirectory()) {
            appdata = appsupport;
        }
        else {
            trace("Bootstrapping %s\n", appSupportName);
            juce::Result r = appsupport.createDirectory();
            if (r.failed()) {
                snprintf(error, sizeof(error), "Directory creation failed: %s\n",
                         CONSTSTRING(r.getErrorMessage()));
                errors.add(juce::String(error));
            }
            else {
                // pretend this was appdata all along
                appdata = appsupport;
            }
        }
#endif
            
        juce::File company = appdata.getChildFile(companyName);
        if (!company.isDirectory()) {
            trace("Bootstrapping %s\n", companyName);
            juce::Result r = company.createDirectory();
            if (r.failed()) {
                snprintf(error, sizeof(error), "Directory creation failed: %s\n",
                         CONSTSTRING(r.getErrorMessage()));
                errors.add(juce::String(error));
            }
        }

        if (company.isDirectory()) {
            juce::File mobius = company.getChildFile(productName);
            if (!mobius.isDirectory()) {
                trace("RootLocator: Bootstrapping %s\n", productName);
                juce::Result r = mobius.createDirectory();
                if (r.failed()) {
                    snprintf(error, sizeof(error), "Directory creation failed: %s\n",
                             CONSTSTRING(r.getErrorMessage()));
                    errors.add(juce::String(error));
                }
            }

            if (mobius.isDirectory()) {
                mobiusinst = mobius;
                // don't look for mobius.xml yet, redirect first
                juce::File alt = checkRedirect(mobius);
                juce::File f = alt.getChildFile("mobius.xml");
                if (f.existsAsFile()) {
                    trace("RootLocator: mobius.xml found: %s\n", PATHSTRING(f));
                    verifiedRoot = alt;
                }
                else {
                    // redirect missing or wrong, look where it normally is
                    f = mobius.getChildFile("mobius.xml");
                    if (f.existsAsFile()) {
                        trace("RootLocator: mobius.xml found: %s\n", PATHSTRING(f));
                        verifiedRoot = mobiusinst;
#ifdef __APPLE__
                        upgradeAppleInstall(mobiusinst, errors);
#endif                        
                    }
                    else {
#ifdef __APPLE__
                        verifiedRoot = bootstrapAppleInstall(mobiusinst, errors);
#endif                        
                    }
                }
            }
        }
    }

    // development hack, look in the usual location for a build environment
    if (verifiedRoot == juce::File()) {
        if (mobiusinst != juce::File())
          trace("RootLocator: Empty installation directory, searching for mobius.xml\n");
#ifdef __APPLE__
        juce::File devroot = juce::File("~/dev/jucetest/UI/Source");
#else
        juce::File devroot = juce::File("c:/dev/jucetest/UI/Source");
#endif
        if (devroot.isDirectory()) {
            trace("RootLocator: Development root found: %s\n", PATHSTRING(devroot));
            verifiedRoot = devroot;
            if (mobiusinst != juce::File()) {
                trace("RootLocator: Bootstrapping mobius-redirect to devroot\n");
                juce::File f = mobiusinst.getChildFile("mobius-redirect");
                bool status = f.replaceWithText(devroot.getFullPathName() + "\n");
                if (!status) {
                    snprintf(error, sizeof(error), "Error creating mobius-redirect\n");
                    errors.add(juce::String(error));
                }
            }
            verifiedRoot = devroot;
        }
    }
        
    if (verifiedRoot == juce::File()) {
        // have to go somewhere
        verifiedRoot = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        snprintf(error, sizeof(error), "Unable to locate root, defaulting to %s\n",
                 PATHSTRING(verifiedRoot));
        errors.add(juce::String(error));
    }

    return verifiedRoot;
}

/**
 * For a new install on Macs, copy over the initial mobius.xml and other
 * system files from the /Applications package to the user's
 * /Library/Application Support
 *
 * Don't need this on Windows since the INNO installer can do more than
 * just a package install.
 */
juce::File RootLocator::bootstrapAppleInstall(juce::File mobiusinst, juce::StringArray& errors)
{
    juce::File verifiedRoot;
    char error[1024];
    
    const char* appdirPath = "/Applications/Mobius.app/Contents/Resources/Install";
    juce::File appdir = juce::File(appdirPath);
    if (!appdir.isDirectory()) {
        // todo: could look in /Library/Audio/Plug-Ins/VST3 for the same shenanigans
        snprintf(error, sizeof(error), "/Applications/Mobius.app was not installed, unable to locate mobius.xml\n");
        errors.add(juce::String(error));
    }
    else {
        trace("RootLocator: Bootstrapping configuration files from /Applications to ~/Library/Application Support\n");
        if (appdir.copyDirectoryTo(mobiusinst)) {
            verifiedRoot = mobiusinst;
        }
        else {
            snprintf(error, sizeof(error), "/Applications/Mobius.app was not copied\n");
            errors.add(juce::String(error));
        }
    }
    return verifiedRoot;
}

/**
 * For an existing install on Macs, make sure the latest system files are copied
 * to the user's /Library/Application Support.
 *
 * It sucks that we have to do this every time.  Really need to make the installer
 * smarter with an after-script or something.
 */
void RootLocator::upgradeAppleInstall(juce::File mobiusinst, juce::StringArray& errors)
{
    const char* appdirPath = "/Applications/Mobius.app/Contents/Resources/Install";
    juce::File appdir = juce::File(appdirPath);
    if (!appdir.isDirectory()) {
        errors.add("/Applications/Mobius.app was not installed, unable to locate mobius.xml\n");
    }
    else {
        upgradeAppleFile("static.xml", appdir, mobiusinst, errors);
        upgradeAppleFile("symbols.xml", appdir, mobiusinst, errors);
        upgradeAppleFile("help.xml", appdir, mobiusinst, errors);
    }
}

void RootLocator::upgradeAppleFile(juce::String name, juce::File appdir, juce::File instdir, juce::StringArray& errors)
{
    juce::File srcfile = appdir.getChildFile(name);
    if (!srcfile.existsAsFile()) {
        errors.add(name + " not found in /Applications/Mobius.app");
    }
    else {
        juce::File destfile = instdir.getChildFile(name);
        if (!destfile.existsAsFile() ||   
            (destfile.getCreationTime() < srcfile.getCreationTime()) ||
            (destfile.getCreationTime() < srcfile.getLastModificationTime())) {
            
            if (!srcfile.copyFileTo(destfile))
              errors.add(name + " could not be copied to Application Support");
            else {
                juce::String msg = juce::String("RootLocator: Copied ") + name +
                    + " to Application Support";
                trace(msg);
            }
        }
    }
}

juce::File RootLocator::checkRedirect(juce::File::SpecialLocationType type)
{
    juce::File f = juce::File::getSpecialLocation(type);
    return checkRedirect(f);
}

juce::File RootLocator::checkRedirect(juce::File root)
{
    juce::File f = root.getChildFile("mobius-redirect");
    if (f.existsAsFile()) {
        trace("RootLocator: Redirect file found %s\n", PATHSTRING(f));
        
        juce::String content = f.loadFileAsString().trim();
        content = findRelevantLine(content);

        if (content.length() == 0) {
            trace("RootLocator: Empty redirect file\n");
        }
        else {
            juce::File redirect;
            if (juce::File::isAbsolutePath(content))
              redirect = juce::File(content);
            else
              redirect = root.getChildFile(content);
        
            if (redirect.isDirectory()) {
                trace("RootLocator: Redirecting to %s\n", PATHSTRING(redirect));

                // recursively redirect again
                // todo: this can cause cycles use a Map to avoid this
                root = checkRedirect(redirect);
            }
            else {
                trace("RootLocator: Redirect file found, but directory does not exist: %s\n",
                      PATHSTRING(redirect));
            }
        }
    }
    
    return root;
}

juce::String RootLocator::findRelevantLine(juce::String src)
{
    juce::String line;

    juce::StringArray tokens;
    tokens.addTokens(src, "\n", "");
    for (int i = 0 ; i < tokens.size() ; i++) {
        juce::String maybe = tokens[i];
        if (!maybe.startsWith("#")) {
            line = maybe;
            break;
        }
    }
    
    return line;
}

//////////////////////////////////////////////////////////////////////
//
// Singelton Interface
//
// Used by Supervisor to cache a copy of the verified root, which
// in retrospect it doesn't need to do, could just call the static
// method and save the File somewhere.
//
//////////////////////////////////////////////////////////////////////

RootLocator::RootLocator()
{
}

RootLocator::~RootLocator()
{
}

juce::File RootLocator::getRoot()
{
    // is this the right way to check for uninitialized?
    if (verifiedRoot == juce::File()) {

        verifiedRoot = getRoot(errors);

        for (auto error : errors)
          trace(error);
    }

    return verifiedRoot;
}

juce::String RootLocator::getRootPath()
{
    return getRoot().getFullPathName();
}

juce::StringArray RootLocator::getErrors()
{
    return errors;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
