/* Eventually this will need to be completely rewritten as Cocoa. -- Cameron Kaiser */

/* Eventually we should localise all the strings too BUT I AM A LAZY AUSTRALIAN-AMERICAN AND I ONLY SPEAK STRINE AND AMERICAN -- Cameron */

/*
    MotifMacLauncher: run a Motif application in X11 on MacOS X
	(C)2009-2010 Cameron Kaiser <ckaiser@floodgap.com>
	
	descended from GIMP.app, which in turn is descended from
    Platypus - create MacOS X application bundles that execute scripts
        This is the executable that goes into Platypus apps
    Copyright (C) 2003 Sveinbjorn Thordarson <sveinbt@hi.is>

    With modifications by Aaron Voisine for gimp.app
    With modifications by Marianne gagnon for Wilber-loves-apple
 
	patched to work on Mac OS X 10.6, (c) 2009 by Simone Karin Lehmann, ( skl at lisanet dor de ) 
	* quiting GIMP by using the context menu now works
	* GIMP (every X11 window) can now be brougth at top by clicking on the icon
	
	The hell it does. It doesn't work at all on Tiger. Changed by Cameron Kaiser for
	MotifMacLauncher for 10.4-10.6 compatibility (C)2009-2010
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    main.c - main program file

*/

/*
 * This app laucher basically takes care of:
 * - launching gimp and X11 when double-clicked
 * - bringing X11 to the top when its icon is clicked in the dock (via a small applescript)
 * - catch file dropped on icon events (and double-clicked gimp documents) and notify gimp.
 * - catch quit events performed outside gimp, e.g. on the dock icon.
 *
 * This Motif version also brings it to the front with Cmd-Tab, and properly juggles needed events.
 */

///////////////////////////////////////
// Includes
///////////////////////////////////////    
#pragma mark Includes

// Apple stuff
#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

// Unix stuff
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>

///////////////////////////////////////
// Definitions
///////////////////////////////////////    
#pragma mark Definitions

// name length limits
#define	kMaxPathLength 1024

// names of files bundled with app
#define kPrereqFileName "prereqs"
#define	kScriptFileName "script"
#define kOpenDocFileName "openDoc"
#define kQuitAppFileName "quitApp"

// custom carbon events
#define kEventClassRedFatalAlert 911
#define kEventKindX11Failed 911
#define kEventKindMotifNotInstalled 912
#define kEventKindNoX11R6 913

//maximum arguments the script accepts 
#define	kMaxArgumentsToScript 252

///////////////////////////////////////
// Prototypes
///////////////////////////////////////    
#pragma mark Prototypes

static void *Execute(void *arg);
static void *OpenDoc(void *arg);
static OSErr ExecuteScript(char *script, pid_t *pid);

static void  GetParameters(void);
static unsigned char* GetPrereqs(void);
static unsigned char* GetScript(void);
static unsigned char* GetOpenDoc(void);
static unsigned char* GetQuitApp(void);

OSErr LoadMenuBar(char *appName);

static OSStatus FSMakePath(FSRef *fileRef, unsigned char *path, long maxPathSize);
static void RedFatalAlert(Str255 errorString, Str255 expStr);
static short DoesFileExist(unsigned char *path);

static OSErr AppQuitAEHandler(const AppleEvent *theAppleEvent,
                              AppleEvent *reply, long refCon);
static OSErr AppOpenDocAEHandler(const AppleEvent *theAppleEvent,
                                 AppleEvent *reply, long refCon);
static OSErr AppOpenAppAEHandler(const AppleEvent *theAppleEvent,
                                 AppleEvent *reply, long refCon);
static OSStatus X11FailedHandler(EventHandlerCallRef theHandlerCall,
                                 EventRef theEvent, void *userData);
static OSStatus MotifFailedHandler(EventHandlerCallRef theHandlerCall,
                                 EventRef theEvent, void *userData);
static OSStatus X11R6FailedHandler(EventHandlerCallRef theHandlerCall,
                                 EventRef theEvent, void *userData);

static OSErr AppReopenAppAEHandler(const AppleEvent *theAppleEvent,
                                   AppleEvent *reply, long refCon);
static pascal OSStatus AppEventHandlerProc(EventHandlerCallRef callRef, EventRef inEvent, void *x);

static OSStatus CompileAppleScript(const void* text, long textLength,
                                  AEDesc *resultData);
static OSStatus SimpleCompileAppleScript(const char* theScript);
static OSErr runScript();

///////////////////////////////////////
// Globals
///////////////////////////////////////    
#pragma mark Globals

// process id of forked process
pid_t pid = 0;

// thread id of threads that start scripts
pthread_t odtid = 0, tid = 0;

// indicator of whether the script has completed executing
short taskDone = true;

// indicator of whether we should NOT switch to X11
short keepFront = true;

// execution parameters
char prereqPath[kMaxPathLength];
char scriptPath[kMaxPathLength];
char openDocPath[kMaxPathLength];
char quitAppPath[kMaxPathLength];

// our reference for our event handler
static EventHandlerRef AppEventHandlerRef;

//arguments to the script
char *arguments[kMaxArgumentsToScript+3];
char *fileArgs[kMaxArgumentsToScript];
short numArgs = 0;

extern char **environ;

#pragma mark -

///////////////////////////////////////
// Program entrance point
///////////////////////////////////////
int main(int argc, const char* argv[])
{
    OSErr err = noErr;
	EventRef event;
    static const EventTypeSpec X11events =
	{ kEventClassRedFatalAlert, kEventKindX11Failed }
	;
	static const EventTypeSpec MotifEvents =
	{ kEventClassRedFatalAlert, kEventKindMotifNotInstalled }
	;
	static const EventTypeSpec X11R6Events =
	{ kEventClassRedFatalAlert, kEventKindNoX11R6 }
	;
   static const EventTypeSpec appControlEvents[] =
   {
   { kEventClassApplication, kEventAppActivated },
   { kEventClassApplication, kEventAppShown }
   };

    //InitCursor();
	
	// we have to install the Event Handlers almost immediately.
	// otherwise we run the risk of synchronization problems.
	
    //install Apple Event handlers
    err += AEInstallEventHandler(kCoreEventClass, kAEQuitApplication,
                                 NewAEEventHandlerUPP(AppQuitAEHandler),
                                 0, false);
    err += AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments,
                                 NewAEEventHandlerUPP(AppOpenDocAEHandler),
                                 0, false);
    err += AEInstallEventHandler(kCoreEventClass, kAEOpenApplication,
                                 NewAEEventHandlerUPP(AppOpenAppAEHandler),
                                 0, false);
    
    err += AEInstallEventHandler(kCoreEventClass, kAEReopenApplication,
                                 NewAEEventHandlerUPP(AppReopenAppAEHandler),
                                 0, false);
    err += InstallEventHandler(GetApplicationEventTarget(),
                               NewEventHandlerUPP(X11FailedHandler), 
							   1, &X11events, 
							   NULL, NULL);
    err += InstallEventHandler(GetApplicationEventTarget(),
                               NewEventHandlerUPP(MotifFailedHandler), 
							   1, &MotifEvents, 
							   NULL, NULL);
    err += InstallEventHandler(GetApplicationEventTarget(),
                               NewEventHandlerUPP(X11R6FailedHandler), 
							   1, &X11R6Events, 
							   NULL, NULL);
		err += InstallEventHandler(GetApplicationEventTarget(),
							   NewEventHandlerUPP(AppEventHandlerProc),
							   GetEventTypeCount(appControlEvents), appControlEvents,
							   0, &AppEventHandlerRef);

    if (err) RedFatalAlert("\pInitialization Error",
                           "\pError initing Apple Event handlers.");
    //create the menu bar
    if (err = LoadMenuBar(NULL)) RedFatalAlert("\pInitialization Error",
                                               "\pError loading MenuBar.nib.");
    
    GetParameters(); //load data from files containing exec settings

	// we must throw errors early, before we split off threads. If we don't,
	// the user will never see the dialog boxes. Thus, do the prereq script.
	
	err = (OSErr)ExecuteScript(prereqPath, &pid);
	// 10 is ok, this means that XQuartz was found instead. 11+ == error
	if ((int)err == 11 || (int)err == 12 || (int)err == 13) {
        CreateEvent(NULL, kEventClassRedFatalAlert, 
					(((int)err == 11) ? kEventKindX11Failed :
					 ((int)err == 12) ? kEventKindMotifNotInstalled : kEventKindNoX11R6), 0,
                    kEventAttributeNone, &event);
        PostEventToQueue(GetMainEventQueue(), event, kEventPriorityStandard);
		taskDone = true;
		// which will be picked up when we get to the main loop
	}
    else {
		taskDone = false;
		// compile "icon clicked" script so it's ready to execute.
		// DON'T COMPILE IT until prereqs pass, because otherwise it tries to
		// find x11.app and might beachball out.
		// The script tells us which one it detected.
		if ((int)err == 10)
		SimpleCompileAppleScript("tell application \"XQuartz\" to activate");
		else
		SimpleCompileAppleScript("tell application \"X11\" to activate");
		keepFront = false;
	}
    RunApplicationEventLoop(); // Run the event loop fail or not so the event gets received.
    return 0;
}
                                 
#pragma mark -

///////////////////////////////////
// Execution thread starts here
///////////////////////////////////
static void *Execute (void *arg)
{

/* REMOVE THIS BEFORE 0.04 */
#if(0)
    EventRef event;
	AppleEvent AEvent, REvent;
	AEAddressDesc appDesc;
	OSStatus err;
	ProcessSerialNumber PSN = {0, kCurrentProcess};
#endif

	OSErr result;

    taskDone = false;
	// keepFront = true;
	result = (OSErr)ExecuteScript(scriptPath, &pid);
	// we can't throw dialogs past this point, so if we had an error, just die off.
    if (result == (OSErr)11 || result == (OSErr)12) {
		taskDone = true;
	} // else {
	//	keepFront = false;
	// }
	
/* REMOVE THIS BEFORE 0.04 */
#if(0)
    if (result == (OSErr)11 || result == (OSErr)12) {
        CreateEvent(NULL, kEventClassRedFatalAlert, 
					((result == (OSErr)11) ? kEventKindX11Failed : kEventKindMotifNotInstalled), 0,
                    kEventAttributeNone, &event);
        PostEventToQueue(GetMainEventQueue(), event, kEventPriorityStandard);
		return 0;
    }	
	keepFront = false; // ok, we're off and running
	// post a reopen event to ourselves make sure the keepFront semaphores are all synchronized throughout.
	err = AECreateDesc(typeProcessSerialNumber, &PSN, sizeof(PSN), &appDesc);
	if (err != noErr) return 0;
	err = AECreateAppleEvent(kCoreEventClass, kAEReopenApplication, &appDesc, kAutoGenerateReturnID, kAnyTransactionID, &AEvent);
	if (err != noErr) return 0;
	err = AESendMessage(&AEvent, &REvent, kAEWaitReply, kNoTimeOut);
	if (err != noErr) return 0;
	(void)AEDisposeDesc(&AEvent);
	(void)AEDisposeDesc(&REvent);
#endif

	ExitToShell();

    return 0;
}

///////////////////////////////////
// Open additional documents thread starts here
///////////////////////////////////
static void *OpenDoc (void *arg)
{
	keepFront = false;
    ExecuteScript(openDocPath, NULL);
    return 0;
}

///////////////////////////////////////
// Run a script via the system command
///////////////////////////////////////
static OSErr ExecuteScript (char *script, pid_t *pid)
{
    pid_t wpid = 0, p = 0;
    int status, i;
    
    if (! pid) pid = &p;
    
    // Generate the array of argument strings before we do any executing
    arguments[0] = script;
    for (i = 0; i < numArgs; i++) arguments[i + 1] = fileArgs[i];
    arguments[i + 1] = NULL;

    *pid = fork(); //open fork
    
    if (*pid == (pid_t)-1) exit(13); //error
    else if (*pid == 0) { //child process started
        execve(arguments[0], arguments, environ);
        exit(13); //if we reach this point, there's an error
    }

    wpid = waitpid(*pid, &status, 0); //wait while child process finishes
    
    if (wpid == (pid_t)-1) return wpid;
    return (OSErr)WEXITSTATUS(status);
}

#pragma mark -

///////////////////////////////////////
// This function loads all the neccesary settings
// from config files in the Resources folder
///////////////////////////////////////
static void GetParameters (void)
{
    char *str;

    if (! (str = (char *)GetPrereqs())) //get path to script to be executed
        RedFatalAlert("\pInitialization Error",
                      "\pError getting script from application bundle.");
    strcpy((char *)&prereqPath, str);
	
    if (! (str = (char *)GetScript())) //get path to script to be executed
        RedFatalAlert("\pInitialization Error",
                      "\pError getting script from application bundle.");
    strcpy((char *)&scriptPath, str);
    
    if (! (str = (char *)GetOpenDoc())) //get path to openDoc
        RedFatalAlert("\pInitialization Error",
                      "\pError getting openDoc from application bundle.");
    strcpy((char *)&openDocPath, str);
    
    if (! (str = (char *)GetQuitApp())) //get path to openDoc
        RedFatalAlert("\pInitialization Error",
                      "\pError getting quitApp from application bundle.");
    strcpy((char *)&quitAppPath, str);
	
}

///////////////////////////////////////
// Get path to the script in Resources folder
///////////////////////////////////////
static unsigned char* GetPrereqs (void)
{
    CFStringRef fileName;
    CFBundleRef appBundle;
    CFURLRef scriptFileURL;
    FSRef fileRef;
    // FSSpec fileSpec;
    unsigned char *path;

    //get CF URL for script
    if (! (appBundle = CFBundleGetMainBundle())) return NULL;
    if (! (fileName = CFStringCreateWithCString(NULL, kPrereqFileName,
                                                kCFStringEncodingASCII)))
        return NULL;
    if (! (scriptFileURL = CFBundleCopyResourceURL(appBundle, fileName, NULL,
                                                   NULL))) return NULL;
    
    //Get file reference from Core Foundation URL
    if (! CFURLGetFSRef(scriptFileURL, &fileRef)) return NULL;
    
    //dispose of the CF variables
    CFRelease(scriptFileURL);
    CFRelease(fileName);
    
    //create path string
    if (! (path = malloc(kMaxPathLength))) return NULL;
    if (FSMakePath(&fileRef, path, kMaxPathLength)) return NULL;
    if (! DoesFileExist(path)) return NULL;
    
    return path;
}


///////////////////////////////////////
// Get path to the script in Resources folder
///////////////////////////////////////
static unsigned char* GetScript (void)
{
    CFStringRef fileName;
    CFBundleRef appBundle;
    CFURLRef scriptFileURL;
    FSRef fileRef;
    // FSSpec fileSpec;
    unsigned char *path;

    //get CF URL for script
    if (! (appBundle = CFBundleGetMainBundle())) return NULL;
    if (! (fileName = CFStringCreateWithCString(NULL, kScriptFileName,
                                                kCFStringEncodingASCII)))
        return NULL;
    if (! (scriptFileURL = CFBundleCopyResourceURL(appBundle, fileName, NULL,
                                                   NULL))) return NULL;
    
    //Get file reference from Core Foundation URL
    if (! CFURLGetFSRef(scriptFileURL, &fileRef)) return NULL;
    
    //dispose of the CF variables
    CFRelease(scriptFileURL);
    CFRelease(fileName);
    
    //convert FSRef to FSSpec
    // if (FSGetCatalogInfo(&fileRef, kFSCatInfoNone, NULL, NULL, &fileSpec, NULL)) return NULL;
        
    //create path string
    if (! (path = malloc(kMaxPathLength))) return NULL;
    if (FSMakePath(&fileRef, path, kMaxPathLength)) return NULL;
    if (! DoesFileExist(path)) return NULL;
    
    return path;
}

///////////////////////////////////////
// Gets the path to openDoc in Resources folder
///////////////////////////////////////
static unsigned char* GetOpenDoc (void)
{
    CFStringRef fileName;
    CFBundleRef appBundle;
    CFURLRef openDocFileURL;
    FSRef fileRef;
    // FSSpec fileSpec;
    unsigned char *path;
    
    //get CF URL for openDoc
    if (! (appBundle = CFBundleGetMainBundle())) return NULL;
    if (! (fileName = CFStringCreateWithCString(NULL, kOpenDocFileName,
                                                kCFStringEncodingASCII)))
        return NULL;
    if (! (openDocFileURL = CFBundleCopyResourceURL(appBundle, fileName, NULL,
                                                    NULL))) return NULL;
    
    //Get file reference from Core Foundation URL
    if (! CFURLGetFSRef( openDocFileURL, &fileRef )) return NULL;
    
    //dispose of the CF variables
    CFRelease(openDocFileURL);
    CFRelease(fileName);
        
    //convert FSRef to FSSpec
    //if (FSGetCatalogInfo(&fileRef, kFSCatInfoNone, NULL, NULL, &fileSpec, NULL)) return NULL;

    //create path string
    if (! (path = malloc(kMaxPathLength))) return NULL;
    // if (FSMakePath(fileSpec, path, kMaxPathLength)) return NULL;
    if (FSMakePath(&fileRef, path, kMaxPathLength)) return NULL;
    if (! DoesFileExist(path)) return NULL;
    
    return path;
}

///////////////////////////////////////
// Gets the path to quitApp in Resources folder
///////////////////////////////////////
static unsigned char* GetQuitApp (void)
{
    CFStringRef fileName;
    CFBundleRef appBundle;
    CFURLRef quitAppFileURL;
    FSRef fileRef;
    unsigned char *path;
    
    //get CF URL for quitApp
    if (! (appBundle = CFBundleGetMainBundle())) return NULL;
    if (! (fileName = CFStringCreateWithCString(NULL, kQuitAppFileName,
                                                kCFStringEncodingASCII)))
        return NULL;
    if (! (quitAppFileURL = CFBundleCopyResourceURL(appBundle, fileName, NULL,
                                                    NULL))) return NULL;
    
    //Get file reference from Core Foundation URL
    if (! CFURLGetFSRef( quitAppFileURL, &fileRef )) return NULL;
    
    //dispose of the CF variables
    CFRelease(quitAppFileURL);
    CFRelease(fileName);
		
    //create path string
    if (! (path = malloc(kMaxPathLength))) return NULL;
	if (FSMakePath(&fileRef, path, kMaxPathLength)) return NULL;
    if (! DoesFileExist(path)) return NULL;
    
    return path;
}

#pragma mark -

/////////////////////////////////////
// Load menu bar from nib
/////////////////////////////////////
OSErr LoadMenuBar (char *appName)
{
    OSErr err;
    IBNibRef nibRef;
    
    if (err = CreateNibReference(CFSTR("MenuBar"), &nibRef)) return err;
    if (err = SetMenuBarFromNib(nibRef, CFSTR("MenuBar"))) return err;
    DisposeNibReference(nibRef);

    return noErr;
}

#pragma mark -

///////////////////////////////////////
// Generate path string from FSSpec record
///////////////////////////////////////
static OSStatus FSMakePath(FSRef *fileRef, unsigned char *path, long maxPathSize)
{
    // OSErr err = noErr;
    // FSRef fileRef;

    //create file reference from file spec
    //if (err = FSpMakeFSRef(&file, &fileRef)) return err;

    // and then convert the FSRef to a path
    return FSRefMakePath(fileRef, path, maxPathSize);
}

////////////////////////////////////////
// Standard red error alert, then exit application
////////////////////////////////////////
static void RedFatalAlert (Str255 errorString, Str255 expStr)
{
	keepFront = true;
    // then remove our activation listener because otherwise we go nuts.
	RemoveEventHandler(AppEventHandlerRef);
    StandardAlert(kAlertStopAlert, errorString,  expStr, NULL, NULL);
    ExitToShell();
}

///////////////////////////////////////
// Determines whether file exists at path or not
///////////////////////////////////////
static short DoesFileExist (unsigned char *path)
{
    if (access((char *)path, F_OK) == -1) return false;
    return true;	
}

#pragma mark -

///////////////////////////////////////
// Apple Event handler for Quit i.e. from
// the dock or Application menu item
///////////////////////////////////////
static OSErr AppQuitAEHandler(const AppleEvent *theAppleEvent,
                              AppleEvent *reply, long refCon)
{
    #pragma unused (reply, refCon, theAppleEvent)

    while (numArgs > 0) free(fileArgs[numArgs--]);
    
	keepFront = true;
    if (! taskDone && pid) { //kill the script process brutally
        // kill(pid, 9);
		ExecuteScript(quitAppPath, NULL);
        // printf("Gimp.app: PID %d killed brutally\n", pid);
    }
    
    pthread_cancel(tid);
    if (odtid) pthread_cancel(odtid);
    
    ExitToShell();
    
    return noErr;
}

/////////////////////////////////////
// Handler for docs dragged on app icon
/////////////////////////////////////
static OSErr AppOpenDocAEHandler(const AppleEvent *theAppleEvent,
                                 AppleEvent *reply, long refCon)
{
    #pragma unused (reply, refCon)
	
    OSErr err = noErr;
    //AEDescList fileSpecList;
    AEDescList fileRefList;
    AEKeyword keyword;
    DescType type;
        
    short i;
    long count, actualSize;
        
    //FSSpec fileSpec;
    FSRef fileRef;
    unsigned char path[kMaxPathLength];
    
    while (numArgs > 0) free(fileArgs[numArgs--]);
        
    //Read the AppleEvent
    //err = AEGetParamDesc(theAppleEvent, keyDirectObject, typeAEList,
    //                     &fileSpecList);
    err = AEGetParamDesc(theAppleEvent, keyDirectObject, typeAEList,
                         &fileRefList);
		
    //err = AECountItems(&fileSpecList, &count); //Count number of files
    err = AECountItems(&fileRefList, &count); //Count number of files
                
    for (i = 1; i <= count; i++) { //iteratively process each file
        //get fsspec from apple event
        //if (! (err = AEGetNthPtr(&fileSpecList, i, typeFSS, &keyword, &type,
		//						(Ptr)&fileSpec, sizeof(FSSpec), &actualSize)))
		if (! (err = AEGetNthPtr(&fileRefList, i, typeFSRef, &keyword, &type,
                                 (Ptr)&fileRef, sizeof(FSRef), &actualSize)))
        {
            //get path from file spec
            //if ((err = FSMakePath(fileSpec, (unsigned char *)&path,
            //                      kMaxPathLength))) return err;
            if ((err = FSMakePath(&fileRef, (unsigned char *)&path, kMaxPathLength))) return err;
                            
            if (numArgs == kMaxArgumentsToScript) break;

            if (! (fileArgs[numArgs] = malloc(kMaxPathLength))) return true;

            strcpy(fileArgs[numArgs++], (char *)&path);
        }
        else return err;
    }
	
	if (keepFront) return err;
	
    if (! taskDone) pthread_create(&odtid, NULL, OpenDoc, NULL);
    else pthread_create(&tid, NULL, Execute, NULL);
        
    return err;
}

///////////////////////////////
// Handler for clicking on app icon
///////////////////////////////
// if app is already open
static OSErr AppReopenAppAEHandler(const AppleEvent *theAppleEvent,
                                 AppleEvent *reply, long refCon)
{
	keepFront = false;
    return runScript();
}

// if app is being opened
static OSErr AppOpenAppAEHandler(const AppleEvent *theAppleEvent,
                                 AppleEvent *reply, long refCon)
{
    #pragma unused (reply, refCon, theAppleEvent)
	
    // the app has been opened without any items dragged on to it
	if (!keepFront)
    pthread_create(&tid, NULL, Execute, NULL);
	
	// don't set keepFront here
	return 0;
}

//////////////////////////////////
// Handler for when X11 fails to start
//////////////////////////////////
static OSStatus X11FailedHandler(EventHandlerCallRef theHandlerCall, 
                                 EventRef theEvent, void *userData)
{
    #pragma unused(theHanderCall, theEvent, userData)
/*
    pthread_join(tid, NULL);
    if (odtid) pthread_join(odtid, NULL);
*/
	keepFront = true;
    RedFatalAlert("\pFailed to start X11",
                  "\pThis application requires Apple X11 or XQuartz. Please install it from your OS X disc or MacOSForge.");

    return noErr;
}
// and one for Motif.
static OSStatus MotifFailedHandler(EventHandlerCallRef theHandlerCall, 
                                 EventRef theEvent, void *userData)
{
    #pragma unused(theHanderCall, theEvent, userData)
/*
    pthread_join(tid, NULL);
    if (odtid) pthread_join(odtid, NULL);
*/
	keepFront = true;
    RedFatalAlert("\pCan't find OpenMotif-Mac",
                  "\pThis application requires OpenMotif-Mac. Please download and install it first.");

    return noErr;
}
// and one for flubbed installs of X11 (such as after an OS upgrade).
static OSStatus X11R6FailedHandler(EventHandlerCallRef theHandlerCall, 
                                 EventRef theEvent, void *userData)
{
    #pragma unused(theHanderCall, theEvent, userData)
/*
    pthread_join(tid, NULL);
    if (odtid) pthread_join(odtid, NULL);
*/
	keepFront = true;
    RedFatalAlert("\pCan't find X11 libraries",
                  "\pPlease reinstall X11 or XQuartz. OS upgrades may cause this error.");

    return noErr;
}

// This handles the situation where we Cmd-Tab to an app, but don't hit it in the Dock
// (i.e., we don't get a ReOpen event).
// EVIL CARBON DO NOT TELL THE GHOST OF STEVE JOBS
static pascal OSStatus AppEventHandlerProc(EventHandlerCallRef callRef, EventRef event, void *x)
{
   UInt32 eventKind = GetEventKind(event);
   switch(eventKind)
   {
      case kEventAppShown:
      case kEventAppActivated:
		if (keepFront)
			return eventNotHandledErr;
		return runScript();
		break;		
   };

return eventNotHandledErr;
}



// Compile and run a small AppleScript. The code below does no cleanup and no proper error checks
// but since it's there until the app is shut down, and since we know the script is okay,
// there should not be any problems.
ComponentInstance theComponent;
AEDesc scriptTextDesc;
OSStatus err;
OSAID scriptID, resultID;

static OSStatus CompileAppleScript(const void* text, long textLength,
                                  AEDesc *resultData) {
    
    resultData = NULL;
    /* set up locals to a known state */
    theComponent = NULL;
    AECreateDesc(typeNull, NULL, 0, &scriptTextDesc);
    scriptID = kOSANullScript;
    resultID = kOSANullScript;
    
    /* open the scripting component */
    theComponent = OpenDefaultComponent(kOSAComponentType,
                                        typeAppleScript);
    if (theComponent == NULL) { err = paramErr; return err; }
    
    /* put the script text into an aedesc */
    err = AECreateDesc(typeChar, text, textLength, &scriptTextDesc);
    if (err != noErr) return err;
    
    /* compile the script */
    err = OSACompile(theComponent, &scriptTextDesc,
                     kOSAModeNull, &scriptID);

    return err;
}

/* runs the compiled applescript */
static OSErr runScript()
{
    /* run the script */
    err = OSAExecute(theComponent, scriptID, kOSANullScript,
                     kOSAModeNull, &resultID);
    return err;
}


/* Simple shortcut to the function that actually compiles the applescript. */
static OSStatus SimpleCompileAppleScript(const char* theScript) {
    return CompileAppleScript(theScript, strlen(theScript), NULL);
}
