/*
** Copyright (C) 2003 Nullsoft, Inc.
**
** This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held 
** liable for any damages arising from the use of this software. 
**
** Permission is granted to anyone to use this software for any purpose, including commercial applications, and to 
** alter it and redistribute it freely, subject to the following restrictions:
**
**   1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. 
**      If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
**
**   2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
**
**   3. This notice may not be removed or altered from any source distribution.
**
*/

#ifndef _LISTVIEW_H_
#define _LISTVIEW_H_

#include <windows.h>
#include <commctrl.h>

class W_ListView 
{
  public:
    W_ListView() { m_libraryparent = NULL; m_hwnd=NULL; m_sort_type=0; m_sort_col=0; m_sort_dir=1; m_col=0; }
    W_ListView(HWND hwnd) { m_libraryparent = NULL; m_hwnd=hwnd; m_col=0; }
    ~W_ListView() { };

    void setwnd(HWND hwnd) { m_hwnd=hwnd; ListView_SetExtendedListViewStyle(hwnd, LVS_EX_FULLROWSELECT|LVS_EX_UNDERLINEHOT ); }

    void AddCol(wchar_t *text, int w);
    int GetCount(void) { return ListView_GetItemCount(m_hwnd); }
    int GetParam(int p);
    void DeleteItem(int n) { ListView_DeleteItem(m_hwnd,n); }
    void Clear(void) { ListView_DeleteAllItems(m_hwnd); }
    int GetSelected(int x) { return (ListView_GetItemState(m_hwnd, x, LVIS_SELECTED) & LVIS_SELECTED)?1:0; }
    int GetSelectionMark() { return ListView_GetSelectionMark(m_hwnd); }
    void SetSelected(int x) { ListView_SetItemState(m_hwnd,x,LVIS_SELECTED,LVIS_SELECTED); }
    int InsertItem(int p, wchar_t *text, int param);
    int InsertItemSorted(wchar_t *text, int param, wchar_t *sorttext);
    void GetItemRect(int i, RECT *r) { ListView_GetItemRect(m_hwnd, i, r, LVIR_BOUNDS); }
    void SetItemText(int p, int si, wchar_t *text);
    void SetItemParam(int p, int param);

    void GetText(int p, int si, wchar_t *text, int maxlen) { ListView_GetItemText(m_hwnd, p, si, text, maxlen); }
    int FindItemByParam(int param)
    {
      LVFINDINFO fi={LVFI_PARAM,0,param};
      return ListView_FindItem(m_hwnd,-1,&fi);
    }
    int FindItemByPoint(int x, int y)
    {
      int l=GetCount();
      for(int i=0;i<l;i++)
      {
        RECT r;
        GetItemRect(i, &r);
        if(r.left<=x && r.right>=x && r.top<=y && r.bottom>=y) return i;
      }
      return -1;
    }
    void SetSortColumn(int col, int bNumeric=0) 
    { 
      if (col == m_sort_col) m_sort_dir=-m_sort_dir;
      else m_sort_dir=1;
      m_sort_col=col; 
      m_sort_type=bNumeric; 
    }
    void SetSortDir(int dir) { m_sort_dir=dir; }
    int GetSortColumn(void) { return m_sort_col; }
    int GetSortDir(void) { return m_sort_dir; }
    int GetColumnWidth(int col);
    void Resort(void);
    HWND getwnd(void) { return m_hwnd; }
        void setLibraryParentWnd(HWND hwndParent)
        {
                m_libraryparent=hwndParent;
        }// for Winamp Font getting stuff

  protected:
    HWND m_hwnd, m_libraryparent;
    int m_col;
    int m_sort_col;
    int m_sort_dir;
    int m_sort_type;
    static int CALLBACK sortfunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort); 
    int _docompare(wchar_t *s1, wchar_t *s2);
};

#endif//_LISTVIEW_H_


