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

#include <windows.h>
#include <commctrl.h>
#include "listview.h"

void W_ListView::AddCol(wchar_t *text, int w)
{
  LVCOLUMN lvc={0,};
  lvc.mask = LVCF_TEXT|LVCF_WIDTH;
  lvc.pszText = text;
  if (w) lvc.cx=w;
  ListView_InsertColumn(m_hwnd, m_col, &lvc);
  m_col++;
}

int W_ListView::GetColumnWidth(int col)
{
  if (col < 0 || col >= m_col) return 0;
  return ListView_GetColumnWidth(m_hwnd,col);
}


int W_ListView::GetParam(int p)
{
  LVITEM lvi={0,};
  lvi.mask = LVIF_PARAM;
  lvi.iItem = p;
  ListView_GetItem(m_hwnd, &lvi);
  return lvi.lParam;
}

int W_ListView::InsertItem(int p, wchar_t *text, int param)
{
  LVITEM lvi={0,};
  lvi.mask = LVIF_TEXT | LVIF_PARAM;
  lvi.iItem = p;
  lvi.pszText = text;
  lvi.cchTextMax=wcslen(text);
  lvi.lParam = param;
  return ListView_InsertItem(m_hwnd, &lvi);
}

int W_ListView::InsertItemSorted(wchar_t *text, int param, wchar_t *sorttext)
{
  int l=GetCount();
  int x;
  for (x = 0; x < l; x ++)
  {
    wchar_t thetext[512];
    GetText(x,m_sort_col,thetext,sizeof(thetext));
    if (_docompare(sorttext,thetext)<0)
    {
      break;
    }
  }
  return InsertItem(x,text,param);
}

void W_ListView::SetItemText(int p, int si, wchar_t *text)
{
  LVITEM lvi={0,};
  lvi.iItem = p;
  lvi.iSubItem = si;
  lvi.mask = LVIF_TEXT;
  lvi.pszText = text;
  lvi.cchTextMax = wcslen(text);
  ListView_SetItem(m_hwnd, &lvi);
}

void W_ListView::SetItemParam(int p, int param)
{
  LVITEM lvi={0,};
  lvi.iItem = p;
  lvi.mask=LVIF_PARAM;
  lvi.lParam=param;
  ListView_SetItem(m_hwnd, &lvi);
}

int W_ListView::_docompare(wchar_t *s1, wchar_t *s2)
{
  if (m_sort_type)
  {
    if(s1[0]==L'>') return m_sort_dir==-1?-1:1;
    if(s2[0]==L'>') return m_sort_dir==-1?1:-1;
    int r=s1[0] - s2[0];
    if (m_sort_dir==-1) return -r;
    return r;
  }
  int r=wcsicmp(s1,s2);
  if (m_sort_dir==-1) return -r;
  return r;
}

int CALLBACK W_ListView::sortfunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
  W_ListView *_this=(W_ListView *)lParamSort;
  int idx1,idx2;
  wchar_t text1[128],text2[128];
  idx1=_this->FindItemByParam(lParam1);
  idx2=_this->FindItemByParam(lParam2);
  if (idx1==-1) return _this->m_sort_dir;
  if (idx2==-1) return -_this->m_sort_dir;
  _this->GetText(idx1,_this->m_sort_col,text1,sizeof(text1));
  _this->GetText(idx2,_this->m_sort_col,text2,sizeof(text2));
  text1[127]=text2[127]=0;
  return _this->_docompare(text1,text2); 
}

void W_ListView::Resort(void)
{
  ListView_SortItems(m_hwnd,sortfunc,(LPARAM)this);
}
