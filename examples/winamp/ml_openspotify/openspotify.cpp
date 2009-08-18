/*
 * This file handles the playlist view that is shown when items
 * this plugin created in the Media Library tree are clicked etc
 *
 * (C) 2009 Noah Williamsson <noah.williamsson at gmail.com>
 *
 */


#include "stdafx.h"
#include "openspotify.h"
#include <stdlib.h>

#include <spotify/api.h>


#define TIMERID	0x123


struct tpMap {
	UINT_PTR treeId;
	int playlist_position;
};
struct _treePlaylistmap {
	int count;
	int size;
	struct tpMap **maps;
};


static LRESULT CALLBACK wndproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void  SP_CALLCONV logged_in(sp_session *session, sp_error error);
static void SP_CALLCONV logged_out(sp_session *session);
static void SP_CALLCONV metadata_updated(sp_session *session);
static void SP_CALLCONV connection_error(sp_session *session, sp_error error);
static void SP_CALLCONV notify_main_thread(sp_session *session);
static void SP_CALLCONV log_message(sp_session *session, const char *data);
static void SP_CALLCONV pc_playlist_added(sp_playlistcontainer *pc, sp_playlist *playlist, int position, void *userdata);
static void SP_CALLCONV pl_tracks_added(sp_playlist *pl, const sp_track **tracks, int num_tracks, int position, void *userdata);
static void SP_CALLCONV playlist_renamed(sp_playlist *pl, void *userdata);
static int openspotify_init(void);


static struct _treePlaylistmap treePlaylistmap;


#include "appkey.c"
#if 0
/* FIXME: Results in unresolved externals, possibly due to C++ name mangling? */
extern const unsigned char g_appkey[];
extern const size_t g_appkey_size;
#endif

/* Our dummy window used for communication */
static HWND m_hWnd;

static HWND hwndLibraryParent;

/* Set by the main thread */
static UINT_PTR m_TreeRootId;


static sp_session *session;



/*
 * Ugly interface to map Media Library tree IDs to playlist
 * positions in the playlist container
 *
 */

void tpAdd(UINT_PTR treeId, int playlist_position) {
	int newcount;
	struct tpMap *tpMap;

	tpMap = (struct tpMap *)malloc(sizeof(struct tpMap));
	if(!tpMap)
		return;

	tpMap->treeId = treeId;
	tpMap->playlist_position = playlist_position;
	if(treePlaylistmap.count == treePlaylistmap.size) {
		newcount = treePlaylistmap.count + 32;
		treePlaylistmap.maps = (struct tpMap **)realloc(treePlaylistmap.maps, sizeof(struct tpMap *) * newcount);
		treePlaylistmap.size = newcount;
	}

	treePlaylistmap.maps[treePlaylistmap.count++] = tpMap;
}


int tpGetPlaylistPos(UINT_PTR treeId) {
	int i;

	for(i = 0; i < treePlaylistmap.count; i++)
		if(treePlaylistmap.maps[i]->treeId == treeId)
			return treePlaylistmap.maps[i]->playlist_position;

	return -1;
}


UINT_PTR tpGetTreeId(int position) {
	int i;

	for(i = 0; i < treePlaylistmap.count; i++)
		if(treePlaylistmap.maps[i]->playlist_position == position)
			return treePlaylistmap.maps[i]->treeId;

	return -1;
}


/*
 * The libopenspotify thread and window loop
 *
 */

DWORD __stdcall openspotify_thread(LPVOID *arg) {
	MSG msg;
	WNDCLASS wcOpenspotify;
	ATOM wc;


	hwndLibraryParent = (HWND)arg;
	DSFYDEBUG("Ping from thread\n");



	/* Create a window to communicate with the plugin, playlist view and Winamp */
	memset(&wcOpenspotify, 0, sizeof(WNDCLASS));
	wcOpenspotify.lpszClassName = L"ml_openspotify";
	wcOpenspotify.lpfnWndProc = wndproc;
	wc = RegisterClass(&wcOpenspotify);

	m_hWnd = CreateWindow(szAppName, L"ml_openspotify message sink", 0, 0, 0, 0, 0, hwndLibraryParent, NULL, 0, 0);
	DSFYDEBUG("Created window at %p\n", m_hWnd);

	if(openspotify_init() < 0) {
		DSFYDEBUG("Failed to initialize libopenspotify\n");
		PostQuitMessage(0);
	}


	DSFYDEBUG("Openspotify initialized, now dispatching messages\n");
	while(GetMessage(&msg, 0, 0, 0) == TRUE) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}


static LRESULT CALLBACK wndproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static char *username, *password;

	switch(uMsg) {
		case WM_TIMER:
			DSFYDEBUG("uMsg=%u (%s), wParam=%u, lParam=%u\n", uMsg, WM2STR(uMsg), wParam, lParam);
			if(session) {
				int timeout;

				KillTimer(hwnd, TIMERID);
				sp_session_process_events(session, &timeout);
				SetTimer(hwnd, TIMERID, timeout, NULL);
			}

			break;

		case WM_USER:
			switch(LOWORD(wParam)) {

			case 1: // Set tree root ID
				m_TreeRootId = (UINT_PTR)lParam;
				break;

			case 2: // Set username
				username = (char *)lParam;
				DSFYDEBUG("Got username '%s'\n", username);
				break;

			case 3: // Set password
				password = (char *)lParam;
				DSFYDEBUG("Got password '%s'\n", password);
				break;

			case 4: // Login if not yet logged in
				if(sp_session_connectionstate(session) != SP_CONNECTION_STATE_LOGGED_IN)
					sp_session_login(session, username, password);
				break;

			case 5: // Get connection status
				if(!session)
					return -1;
				else if(sp_session_connectionstate(session) != SP_CONNECTION_STATE_LOGGED_IN)
					return -1;
				return 0;
			}
			break;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}



/*
 * Spotify API specific stuff starts here!
 *
 */
static void SP_CALLCONV connection_error(sp_session *session, sp_error error) {
	DSFYDEBUG("SESSION CALLBACK\n");
	fprintf(stderr, "connection to Spotify failed: %s\n",
	                sp_error_message(error));

}


static void SP_CALLCONV logged_in(sp_session *session, sp_error error) {
	sp_playlistcontainer *pc;
	static sp_playlistcontainer_callbacks callbacks;

	DSFYDEBUG("SESSION CALLBACK\n");
	if (SP_ERROR_OK != error) {
		fprintf(stderr, "failed to log in to Spotify: %s\n",
		                sp_error_message(error));

		return;
	}


	/* Snoop on added playlists */
	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.playlist_added = pc_playlist_added;
	callbacks.playlist_moved = NULL;
	callbacks.playlist_removed = NULL;

	pc = sp_session_playlistcontainer(session);
	sp_playlistcontainer_add_callbacks(pc, &callbacks, NULL);
}


static void SP_CALLCONV logged_out(sp_session *session) {
	DSFYDEBUG("SESSION CALLBACK\n");

}


static void SP_CALLCONV notify_main_thread(sp_session *session) {
	DSFYDEBUG("SESSION CALLBACK\n");

	PostMessage(m_hWnd, WM_TIMER, TIMERID, 0);
}


static void SP_CALLCONV metadata_updated(sp_session *session) {

	DSFYDEBUG("SESSION CALLBACK\n");
}


static void SP_CALLCONV log_message(sp_session *session, const char *data) {
	DSFYDEBUG("SESSION CALLBACK\n");
	fprintf(stderr, "log_message: %s\n", data);
}


static void SP_CALLCONV pc_playlist_added(sp_playlistcontainer *pc, sp_playlist *playlist, int position, void *userdata) {
	static sp_playlist_callbacks *callbacks;
	MLTREEITEM item;

	if(callbacks == NULL) {
		callbacks = (sp_playlist_callbacks *)malloc(sizeof(sp_playlist_callbacks));
		memset(callbacks, 0, sizeof(sp_playlist_callbacks));

		callbacks->tracks_added = pl_tracks_added;
		callbacks->playlist_renamed = playlist_renamed;
	}

	/* Snoop on the playlist to know when it's named and tracks are added */
	sp_playlist_add_callbacks(playlist, callbacks, NULL);


	/* Add playlist to our Media Library tree */
	item.size = sizeof(MLTREEITEM);
	item.parentId		= m_TreeRootId;
	item.hasChildren	= 0;
	item.id				= 0;
	item.imageIndex		= 0;
	item.title			= "(Loading...)";
	
	/* Create a new entry in under our tree in the Media Library pane */
	SendMessage(hwndLibraryParent, WM_ML_IPC, (WPARAM) &item, ML_IPC_TREEITEM_ADD);
	DSFYDEBUG("New id of entry '%s' is '%u'\n", item.title, item.id);

	/* Add mapping between tree ID and playlist position in playlist container */
	tpAdd(item.id, position);
}


void SP_CALLCONV playlist_renamed(sp_playlist *pl, void *userdata) {
	sp_playlistcontainer *pc = sp_session_playlistcontainer(session);
	int num = sp_playlistcontainer_num_playlists(pc);
	int pos;
	MLTREEITEMINFO treeItemInfo;
	UINT_PTR treeId, treeHandle;


	/* Find position of renamed playlist in the playlist container */
	for(pos = 0; pos < num; pos++)
		if(sp_playlistcontainer_playlist(pc, pos) == pl)
			break;

	if(pos == num) {
		DSFYDEBUG("WTF, did not find playlist %p in container!\n", pl);
		return;
	}


	/* Get tree ID so we can get the handle */
	treeId = tpGetTreeId(pos);
	treeHandle = SendMessage(hwndLibraryParent, WM_ML_IPC, (WPARAM)treeId, ML_IPC_TREEITEM_GETHANDLE);

	/* Create request to update the item */
	memset(&treeItemInfo, 0, sizeof(MLTREEITEMINFO));
	treeItemInfo.handle = treeHandle;
	treeItemInfo.mask = MLTI_TEXT;
	treeItemInfo.item.size = sizeof(MLTREEITEM);
	treeItemInfo.item.title = (char *)sp_playlist_name(pl);

	/* Submit request */
	SendMessage(hwndLibraryParent, WM_ML_IPC, (WPARAM)&treeItemInfo, ML_IPC_TREEITEM_SETINFO);
}


void SP_CALLCONV pl_tracks_added(sp_playlist *pl, const sp_track **tracks, int num_tracks, int position, void *userdata) {
	DSFYDEBUG("Playlist '%s': %d tracks added\n", sp_playlist_name(pl), num_tracks);

	/*
	 * FIXME: Actually do something here ;)
	 * GetDlgItem(.., IDC_LIST), issue a ListView_RedrawIterms() and handle
	 * WM_NOTIFY messages in the ListView's parent window to update its entries
	 *
	 */
}


/*
 * Initialize the Spotify session object, called by openspotify_thread()
 *
 */
static int openspotify_init(void) {
	sp_session_config config;
	sp_error error;
	sp_session_callbacks callbacks = {
		&logged_in,
		&logged_out,
		&metadata_updated,
		&connection_error,
		NULL,
		&notify_main_thread,
		NULL,
		NULL,
		&log_message
	};


	memset(&config, 0, sizeof(config));
	config.api_version = SPOTIFY_API_VERSION;
	config.cache_location = "tmp";
	config.settings_location = "tmp";
	config.application_key = g_appkey;
	config.application_key_size = g_appkey_size;
	config.user_agent = "ml_openspotify";
	config.callbacks = &callbacks;


	error = sp_session_init(&config, &session);
	if(error != SP_ERROR_OK) {
		DSFYDEBUG("sp_session_init() failed with error '%s'\n", sp_error_message(error));
		return -1;
	}


	return 0;
}
