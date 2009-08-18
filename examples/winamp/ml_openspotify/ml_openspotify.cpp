/*
 * Initialization and main entry point for the media library plugin
 *
 * (C) 2009 Noah Williamsson <noah.williamsson at gmail.com>
 *
 */


#include <stdio.h>
#include "stdafx.h"

#include "openspotify.h"
#include "view.h"
#include "config.h"


static int osfy_init(void);
static void osfy_release(void);
static int osfy_treeview(void);
static int osfy_msgproc(int message_type, int param1, int param2, int param3);


/*
 * The root tree item of our "Spotify" item
 * Needed by the plugin message proc to which tree item was clicked etc
 *
 */
static UINT_PTR m_TreeRootId;


HANDLE hThread;
DWORD thrId;

/*
 * Our plugin definition
 * Found by Winamp when the DLL is loaded
 *
 */
winampMediaLibraryPlugin plugin = {
	MLHDR_VER,
	"Openspotify Media Library plugin",
	osfy_init,
	osfy_release,
	osfy_msgproc,
	0,
	0,
	0,
};

extern "C"
__declspec(dllexport)
winampMediaLibraryPlugin *winampGetMediaLibraryPlugin() {
	return &plugin;
}


/* Plugin initialization */
static int osfy_init(void) {

#ifdef _DEBUG
	if(AllocConsole()) {
		freopen("CONOUT$", "wt", stdout);
		freopen("CONOUT$", "wt", stderr);
		SetConsoleTitle(L"ml_openspotify debug console");
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED);
	}
#endif

	/* Create root node in tree, for playlists */
	osfy_treeview();

	/* Create standalone thread to run the libopenspotify session */
	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)openspotify_thread, plugin.hwndLibraryParent, 0, &thrId);
	DSFYDEBUG("Initialized, new thread running at %p\n", hThread);

	return 0;
}


/* To release resources when the plugin is unloaded or Winamp exits */
static void osfy_release(void) {
	HWND hwndThread;

	hwndThread = FindWindow(szAppName, NULL);
	DSFYDEBUG("Posting WM_DESTROY to thread with window %p\n", hwndThread);
	if(hwndThread)
		PostMessage(hwndThread, WM_DESTROY, 0, 0);

	Sleep(300);
	TerminateThread(hThread, 0);

#ifdef _DEBUG
	FreeConsole();
#endif
}


/* The plugin message handler */
static int osfy_msgproc(int message_type, int param1, int param2, int param3) {
	HWND hwndView, hwndThread;

	if (message_type == ML_MSG_TREE_ONCREATEVIEW) {

		/* Display playlist view */
		hwndView = CreateDialog(plugin.hDllInstance, 
						MAKEINTRESOURCE(IDD_VIEW_PLAYLIST),
						(HWND)(LONG_PTR)param2,
						(DLGPROC)dlgproc);
		DSFYDEBUG("Created playlist view at %p for tree item with id 0x%08x\n", hwndView, (unsigned int)param1);

		if(param1 == m_TreeRootId) {

			hwndThread = FindWindow(szAppName, NULL);
			if(hwndThread) {
				/* Notify openspotify thread about the playlist view and the root ID */
#if 0 
				SendMessage(hwndThread, WM_USER, 0x0000, (UINT_PTR)hwndView);
#endif
				SendMessage(hwndThread, WM_USER, 0x0001, (UINT_PTR)m_TreeRootId);

				/* If not logged in yet, popup configuration dialog */
				DSFYDEBUG("Checking with HWND %p if logged in..\n", hwndThread);
				if(SendMessage(hwndThread, WM_USER, 0x0005, 0) == -1) {
					config_dialog(plugin.hDllInstance, plugin.hwndLibraryParent);
					DSFYDEBUG("config_dialog() returned\n");
				}
			}
		}

		return (INT_PTR)hwndView;
	}

	return 0;
}


/* Help function to create a node in the Media Library tree */
static int osfy_treeview(void) {
	MLTREEITEM treeroot;

	treeroot.size = sizeof(MLTREEITEM);
	treeroot.parentId		= 0;
	treeroot.title			= "Spotify";
	treeroot.hasChildren	= 0;
	treeroot.id				= 0;
	treeroot.imageIndex		= 0;

	/* Create a new tree in the Media Library pane */
	SendMessage(plugin.hwndLibraryParent, WM_ML_IPC, (WPARAM) &treeroot, ML_IPC_TREEITEM_ADD);
	m_TreeRootId = treeroot.id;

	return 0;
}
