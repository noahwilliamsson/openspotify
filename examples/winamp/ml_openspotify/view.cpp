/*
 * This file handles the playlist view that is shown when items
 * this plugin created in the Media Library tree are clicked etc
 *
 * (C) 2009 Noah Williamsson <noah.williamsson at gmail.com>
 *
 */

#include "stdafx.h"
#include "config.h"


/* Function pointers we're pulling out of Winamp */
static int (*wad_getColor)(int idx);
static int (*wad_handleDialogMsgs)(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void (*wad_DrawChildWindowBorders)(HWND hwndDlg, int *tab, int tabsize);


/* ..listview.cpp wants access to the parent window of the plugin */
extern winampMediaLibraryPlugin plugin;

/* Dialog window for our playlist view, updated by WM_INITDIALOG */
static HWND m_hWnd;

/* Dummy window for the libopenspotify thread to sink messages to */
static HWND m_hWndOpenSpotify;

/* For skinning the list view */
static int m_listview_skinning_handle;

/* Listview for songs in a playlist */
static W_ListView m_list;


/* Use helper code in childwnd.h to get a pretty, resizable view */
static void (*cr_init)(HWND hwndDlg, ChildWndResizeItem *list, int num);
static void (*cr_resize)(HWND hwndDlg, ChildWndResizeItem *list, int num);
static ChildWndResizeItem resize_rlist[] = {
	/*
	 * The bit field represents what corners to max out
	 * The corners are: left (0x1000), top (0x0100), right (0x0010) and bottom (0x0001)
	 */
	{ IDC_QUICKSEARCH,	0x0010 /* Update right position (width) */ },
	{ IDC_LIST,		0x0011 /* Update right position (width) and bottom-left corner */ },
	{ IDC_BUTTON_CONFIG,	0x0101 /* Grow/shrink top and bottom */ },
	{ IDC_STATUS,		0x0111 /* Grow/shrink top, right and bottom */ }
};


/* Dialog message handler */
LRESULT CALLBACK dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static wchar_t buf[100];
	LPNMHDR nmhdr;

	/* Run Winamp dialog proc */
	if(0 && wad_handleDialogMsgs) {
		BOOL ret;

		ret = wad_handleDialogMsgs(hwndDlg, uMsg, wParam, lParam);
		if(ret)
			return ret;
	}


	switch(uMsg) {
		case WM_INITDIALOG:
			DSFYDEBUG("uMsg=%u (%s), wParam=%u, lParam=%u\n", uMsg, WM2STR(uMsg), wParam, lParam);
			/* Make the dialog HWND global */
			m_hWnd = hwndDlg;


			/* Get function pointers for some resizing functions in Winamp */
			*(void **)&cr_init = (void *)SendMessage(
				plugin.hwndLibraryParent, WM_ML_IPC,
				32, ML_IPC_SKIN_WADLG_GETFUNC);

			*(void **)&cr_resize = (void *)SendMessage(
				plugin.hwndLibraryParent, WM_ML_IPC,
				33, ML_IPC_SKIN_WADLG_GETFUNC);


			/* Initialize the listview wrapper */
			//m_list.setLibraryParentWnd(plugin.hwndLibraryParent);
			/* Grab HWND of the ListView control (IDC_LIST) */
			m_list.setwnd(GetDlgItem(hwndDlg, IDC_LIST));


			/* Pull some skinning related function pointers out of Winamp */
			*(void **)&wad_getColor = (void *)SendMessage(
				plugin.hwndLibraryParent, WM_ML_IPC,
				1, ML_IPC_SKIN_WADLG_GETFUNC);

			*(void **)&wad_handleDialogMsgs = (void *)SendMessage(
				plugin.hwndLibraryParent, WM_ML_IPC,
				2, ML_IPC_SKIN_WADLG_GETFUNC);

			*(void **)&wad_DrawChildWindowBorders = (void *)SendMessage(
				plugin.hwndLibraryParent, WM_ML_IPC,
				3, ML_IPC_SKIN_WADLG_GETFUNC);
      

			/* Use colors according to the skin for the ListView */
			ListView_SetTextColor(m_list.getwnd(),
				wad_getColor? wad_getColor(WADLG_ITEMFG): RGB(0xff,0xff,0xff));
			ListView_SetBkColor(m_list.getwnd(),
				wad_getColor? wad_getColor(WADLG_ITEMBG): RGB(0x00,0x00,0x00));
			ListView_SetTextBkColor(m_list.getwnd(),
				wad_getColor? wad_getColor(WADLG_ITEMBG):RGB(0x00,0x00,0x00));

			

			/* Skin listview */
			m_listview_skinning_handle = SendMessage(plugin.hwndLibraryParent, WM_ML_IPC,
							(int)m_list.getwnd(),ML_IPC_SKIN_LISTVIEW);



			/* Add a few columns to our listview */
			m_list.AddCol(L"Track", 150);
			m_list.AddCol(L"Artist", 100);
			m_list.AddCol(L"Length", 50);
			m_list.AddCol(L"Album", 100);

			m_list.InsertItem(0, L"KNARK0.1", 1);

			/*
			m_list.InsertItem(0, L"KNARK0.1", 1);
			m_list.InsertItem(1, L"KNARK1.2", 2);
			m_list.InsertItem(2, L"KNARK2.1", 1);
			m_list.InsertItem(3, L"KNARK3.1", 1);
			m_list.InsertItem(4, L"KNARK4.1", 1);
			m_list.InsertItem(5, L"KNARK5.1", 1);
			*/
			/* Resize the elements in our dialog */
			if(cr_init) {
				DSFYDEBUG("Resizing items in dialog\n");
				cr_init(hwndDlg, resize_rlist, sizeof(resize_rlist) / sizeof(resize_rlist[0]));
			}
			break;


		case WM_DESTROY:
			DSFYDEBUG("uMsg=%u (%s), wParam=%u, lParam=%u\n", uMsg, WM2STR(uMsg), wParam, lParam);
			/* Unskin listview */
			SendMessage(plugin.hwndLibraryParent, WM_ML_IPC,
						m_listview_skinning_handle, ML_IPC_UNSKIN_LISTVIEW);

			/* NULL variables again */
			m_hWnd = NULL;
			wad_handleDialogMsgs = NULL;
			cr_init = NULL;
			cr_resize = NULL;
			break;


		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDC_BUTTON_CONFIG:
					DSFYDEBUG("uMsg=%u (%s), wParam=%u, lParam=%u\n", uMsg, WM2STR(uMsg), wParam, lParam);
					config_dialog(plugin.hDllInstance, hwndDlg);
					break;

				case EN_CHANGE:
					/* Someone is editing the search query box */
#if 0
					/* Create a WM_TIMER message */
					KillTimer(hwndDlg, 0x242);
					SetTimer(hwndDlg, 0x242, 500, NULL);
#endif
					break;
				default:
					break;
			}
			break;


		case WM_SIZE:
			DSFYDEBUG("uMsg=%u (%s), wParam=%u, lParam=%u\n", uMsg, WM2STR(uMsg), wParam, lParam);
			if(wParam == SIZE_MINIMIZED)
				break;
			else if(cr_resize == NULL)
				break;

			DSFYDEBUG("Resizing items in dialog on WM_SIZE\n");
			/* FIXME: Grrr.. doesn't resize the listview for some weird reason */
			cr_resize(hwndDlg, resize_rlist, sizeof(resize_rlist) / sizeof(resize_rlist[0]));
			break;


		case WM_PAINT:
			DSFYDEBUG("uMsg=%u (%s), wParam=%u, lParam=%u\n", uMsg, WM2STR(uMsg), wParam, lParam);
			{
				int tab[] = {
						IDC_QUICKSEARCH | DCW_SUNKENBORDER,
						IDC_LIST | DCW_SUNKENBORDER
				};

				if(wad_DrawChildWindowBorders)
					wad_DrawChildWindowBorders(hwndDlg, tab, 2);
			}
			break;


		case WM_NOTIFY:
			DSFYDEBUG("uMsg=%u (%s), wParam=%u, lParam=%u\n", uMsg, WM2STR(uMsg), wParam, lParam);
			nmhdr = (LPNMHDR)lParam;
			wsprintf(buf, L"yGot WM_NOTIFY with idFrom=%d\n", nmhdr->idFrom);
			break;


		default:
			break;
	}


	return 0;
}

