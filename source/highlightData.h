/*******************************************************************************
*                                                                              *
* highlightData.h -- Nirvana Editor Highlight Data Header File                 *
*                                                                              *
* Copyright 2003 The NEdit Developers                                          *
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

#ifndef HIGHLIGHT_DATA_H_
#define HIGHLIGHT_DATA_H_

#include "highlight.h"
#include "nedit.h"
#include "string_view.h"
#include <string>

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>

XFontStruct *FontOfNamedStyle(WindowInfo *window, view::string_view styleName);
bool LoadHighlightString(const char *inString, int convertOld);
bool LoadStylesString(const char *inString);
bool NamedStyleExists(view::string_view styleName);
char *WriteHighlightString(void);
char *WriteStylesString(void);
int FontOfNamedStyleIsBold(view::string_view styleName);
int FontOfNamedStyleIsItalic(view::string_view styleName);
int IndexOfNamedStyle(view::string_view styleName);
int LMHasHighlightPatterns(view::string_view languageMode);
patternSet *FindPatternSet(view::string_view langModeName);
std::string BgColorOfNamedStyleEx(view::string_view styleName);
std::string ColorOfNamedStyleEx(view::string_view styleName);
std::string WriteHighlightStringEx(void);
std::string WriteStylesStringEx(void);
void EditHighlightPatterns(WindowInfo *window);
void EditHighlightStyles(const char *initialStyle);
void RenameHighlightPattern(const char *oldName, const char *newName);
void UpdateLanguageModeMenu(void);

#endif
