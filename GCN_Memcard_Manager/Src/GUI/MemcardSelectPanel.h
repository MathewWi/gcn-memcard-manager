// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef __MEMCARD_SELECT_PANEL_h__
#define __MEMCARD_SELECT_PANEL_h__

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/filepicker.h>

#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/listctrl.h>
#include <wx/imaglist.h>
#include <wx/fontmap.h>

class CMemcardSelectPanel : public wxDialog
{
public:
	CMemcardSelectPanel(wxWindow * parent, wxString path = wxEmptyString, bool resizeOnly = false);
	~CMemcardSelectPanel();

	wxString GetSelectionModal();
private:
	DECLARE_EVENT_TABLE();
	void CreateGUIControls();
	void OnPathChange(wxFileDirPickerEvent& event) { Update();}
	void Update();
	wxString ResizeMemcard();
	wxString CreateMemcard();
 
	wxFilePickerCtrl * MemcardPath;
	wxChoice * blocksChoice,
			 * regionChoice;
	wxButton * createOpenButton;
	bool m_resizeOnly;
};

#endif