/*
 * This file provides a rudimentary username/password configuration dialog
 *
 * (C) 2009 Noah Williamsson <noah.williamsson at gmail.com>
 *
 */


#include "stdafx.h"
#include "openspotify.h"


static LRESULT CALLBACK config_dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam,LPARAM lParam);


int config_dialog(HINSTANCE hInst, HWND hWndParent) {
	CreateDialog(hInst, MAKEINTRESOURCE(IDD_CONFIG),
		hWndParent, (DLGPROC)config_dlgproc);

	return 0;
}


static LRESULT CALLBACK config_dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static char username[256] = { "blabla" }, password[256] = { "" };
	wchar_t buf[256];
	HWND hwndThread;


	switch(uMsg) {
		case WM_INITDIALOG:
			DSFYDEBUG("uMsg=%u (%s), wParam=%u, lParam=%u\n", uMsg, WM2STR(uMsg), wParam, lParam);
			memset(buf, 0, sizeof(buf));
			MultiByteToWideChar(CP_UTF8, 0, username, -1, buf, 255);
			SetDlgItemText(hwndDlg, IDC_USERNAME, buf);

			memset(buf, 0, sizeof(buf));
			MultiByteToWideChar(CP_UTF8, 0, password, -1, buf, 255);
			SetDlgItemText(hwndDlg, IDC_PASSWORD, buf);
			break;

		case WM_DESTROY:
			DSFYDEBUG("uMsg=%u (%s), wParam=%u, lParam=%u\n", uMsg, WM2STR(uMsg), wParam, lParam);
			break;

		case WM_COMMAND:
			DSFYDEBUG("uMsg=%u (%s), wParam=%u, lParam=%u\n", uMsg, WM2STR(uMsg), wParam, lParam);
			switch(LOWORD(wParam)) {
				case IDOK:
					memset(buf, 0, sizeof(buf));
					memset(username, 0, sizeof(username));
					GetDlgItemText(hwndDlg, IDC_USERNAME, buf, 255);
					WideCharToMultiByte(CP_UTF8, 0, buf, 255, username, sizeof(username), NULL, NULL);

					memset(buf, 0, sizeof(buf));
					memset(password, 0, sizeof(password));
					GetDlgItemText(hwndDlg, IDC_PASSWORD, buf, 255);
					WideCharToMultiByte(CP_UTF8, 0, buf, 255, password, sizeof(password), NULL, NULL);

					hwndThread = FindWindow(szAppName, NULL);
					if(hwndThread && *username && *password) {
						/* Pass pointers to static buffers */
						SendMessage(hwndThread, WM_USER, 0x0002, (LPARAM)username);
						SendMessage(hwndThread, WM_USER, 0x0003, (LPARAM)password);

						/* Attempt login */
						SendMessage(hwndThread, WM_USER, 0x0004, 0);
					}			

				case IDCANCEL:
					EndDialog(hwndDlg, wParam);
					break;
			}

			break;
	}

	return 0;
}
