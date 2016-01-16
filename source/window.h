/*******************************************************************************
*                                                                              *
* window.h -- Nirvana Editor Window header file                                *
*                                                                              *
* Copyright 2004 The NEdit Developers                                          *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for    *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* July 31, 2001                                                                *
*                                                                              *
*******************************************************************************/

#ifndef WINDOW_H_
#define WINDOW_H_

#include "nedit.h"
#include "TextBuffer.h"

#include <X11/Intrinsic.h>

WindowInfo *CreateDocument(WindowInfo *shellWindow, const char *name);
WindowInfo *DetachDocument(WindowInfo *window);
WindowInfo *FindWindowWithFile(const char *name, const char *path);
WindowInfo *GetTopDocument(Widget w);
WindowInfo *MarkActiveDocument(WindowInfo *window);
WindowInfo *MarkLastDocument(WindowInfo *window);
WindowInfo *MoveDocument(WindowInfo *toWindow, WindowInfo *window);
WindowInfo *TabToWindow(Widget tab);
WindowInfo *WidgetToWindow(Widget w);
int GetSimpleSelection(TextBuffer *buf, int *left, int *right);
int NWindows(void);
int WidgetToPaneIndex(WindowInfo *window, Widget w);
void AddSmallIcon(Widget shell);
void AllWindowsBusy(const char *message);
void AllWindowsUnbusy(void);
void AttachSessionMgrHandler(Widget appShell);
void SetBacklightChars(WindowInfo *window, char *applyBacklightTypes);
void SetColors(WindowInfo *window, const char *textFg, const char *textBg, const char *selectFg, const char *selectBg, const char *hiliteFg, const char *hiliteBg, const char *lineNoFg, const char *cursorFg);
void SetEmTabDist(WindowInfo *window, int emTabDist);
void SetFonts(WindowInfo *window, const char *fontName, const char *italicName, const char *boldName, const char *boldItalicName);
void SetModeMessage(WindowInfo *window, const char *message);
void SetOverstrike(WindowInfo *window, int overstrike);
void SetSensitive(WindowInfo *window, Widget w, Boolean sensitive);
void SetShowMatching(WindowInfo *window, int state);
void SetTabDist(WindowInfo *window, int tabDist);
void SetToggleButtonState(WindowInfo *window, Widget w, Boolean state, Boolean notify);
void SetWindowModified(WindowInfo *window, int modified);
void ShowISearchLine(WindowInfo *window, int state);
void ShowLineNumbers(WindowInfo *window, int state);
void ShowStatsLine(WindowInfo *window, int state);
void ShowTabBar(WindowInfo *window, int state);
void ShowWindowTabBar(WindowInfo *window);
void SortTabBar(WindowInfo *window);
void SplitPane(WindowInfo *window);
void TempShowISearch(WindowInfo *window, int state);


#endif
