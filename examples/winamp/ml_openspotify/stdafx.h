// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <commctrl.h>

// TODO: reference additional headers your program requires here
#ifdef _DEBUG
#include <stdio.h>
#endif
#include <string.h>

// From Winamp SDK in C:\Program\Winamp SDK
#include "winamp/wa_dlg.h"
#include "gen_ml/ml.h"
#include "gen_ml/childwnd.h"

// Dialog and listview resources
#include "resource.h"
#include "listview.h"

// Debugging stuff
#include "debug.h"
