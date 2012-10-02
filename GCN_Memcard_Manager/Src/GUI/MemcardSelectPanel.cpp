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
#include "MemcardManager.h"
#include "MemcardSelectPanel.h"
#include "wx/spinctrl.h"
BEGIN_EVENT_TABLE(CMemcardSelectPanel, wxDialog)
	EVT_FILEPICKER_CHANGED(wxID_ANY,CMemcardSelectPanel::OnPathChange)
END_EVENT_TABLE()

CMemcardSelectPanel::CMemcardSelectPanel(wxWindow * parent, wxString path, bool resizeOnly)
	: wxDialog(parent, wxID_ANY, _("New Memory Card"),wxDefaultPosition, wxSize(200,200))
	, m_resizeOnly(resizeOnly)
{
	CreateGUIControls();
	if (path != wxEmptyString)
		MemcardPath->SetPath(path);
	Update();
}

CMemcardSelectPanel::~CMemcardSelectPanel()
{
}

wxString CMemcardSelectPanel::GetSelectionModal()
{
	switch (ShowModal())
	{
	case wxID_CANCEL:
		return wxEmptyString;
	case wxID_OK:
		if (m_resizeOnly)
			return ResizeMemcard();
		return CreateMemcard();
	case wxID_IGNORE:
		return MemcardPath->GetPath();
	}
	return wxEmptyString;
}


wxString CMemcardSelectPanel::ResizeMemcard()
{
	std::string filename = (std::string)MemcardPath->GetPath().mb_str();

	if (!File::IsDirectory(filename))
	{	
		int size = MemCard59Mb << blocksChoice->GetSelection();
		GCMemcard Memcard(filename.c_str());
		if (Memcard.IsValid())
		{
			if (Memcard.ChangeMemoryCardSize(size))
				return wxString(filename.c_str(), *wxConvCurrent);
		}
	}
	return wxEmptyString;
}

wxString CMemcardSelectPanel::CreateMemcard()
{
	std::string filename = (std::string)MemcardPath->GetPath().mb_str();

	if (!File::IsDirectory(filename))
	{	
		bool sjis = regionChoice->GetSelection()==2;
		int size = MemCard59Mb << blocksChoice->GetSelection();
		GCMemcard Memcard(filename.c_str(), true, sjis, size);
		if (Memcard.IsValid())
		{
			return wxString(filename.c_str(), *wxConvCurrent);
		}
	}
	return wxEmptyString;
}

void CMemcardSelectPanel::CreateGUIControls()
{
	wxStaticText * mainTxt = new wxStaticText(this, wxID_ANY, _("Choose a Memory Card File"));
	
	wxArrayString sizesArray;
	for (int i = MemCard59Mb; i <= MemCard2043Mb; i<<=1)
	{
		sizesArray.Add(wxString::Format(wxT("%d"), (i*MBIT_TO_BLOCKS)-5));
	}
	wxStaticText * blocksTxt = new wxStaticText(this, wxID_ANY, _("Size"));
	blocksChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, sizesArray);
	blocksChoice->SetSelection(5);

	wxArrayString regionArray;
	regionArray.Add(wxT("USA"));
	regionArray.Add(wxT("PAL"));
	regionArray.Add(wxT("JAP"));
	wxStaticText * regionTxt = new wxStaticText(this, wxID_ANY, _("Region"));
	regionChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, regionArray);
	regionChoice->SetSelection(0);
	


	MemcardPath = new wxFilePickerCtrl(this, wxID_ANY,
			 wxString::From8BitData(File::GetUserPath(D_GCUSER_IDX).c_str()), _("Save memory card as..."),
		_("Gamecube Memory Cards (*.raw,*.gcp,*.mci)") + wxString(wxT("|*.raw;*.gcp;*.mci")), wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL|wxFLP_SAVE);
	
	wxBoxSizer * const memcardSettingsSizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer * const choiceSizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer * const mainSizer = new wxBoxSizer(wxVERTICAL);

	memcardSettingsSizer->Add(blocksTxt,  0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
	memcardSettingsSizer->Add(blocksChoice,  0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
	memcardSettingsSizer->AddSpacer(1);
	memcardSettingsSizer->Add(regionTxt,  0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
	memcardSettingsSizer->Add(regionChoice,  0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
	memcardSettingsSizer->AddSpacer(1);
	memcardSettingsSizer->Add(MemcardPath,  0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
	createOpenButton = new wxButton(this, wxID_OK, _("Create"));
	choiceSizer->Add(createOpenButton,  0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
	choiceSizer->Add(new wxButton(this, wxID_CANCEL, _("Cancel")),  0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

	mainSizer->Add(mainTxt,  0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
	mainSizer->Add(memcardSettingsSizer,  0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
	mainSizer->Add(choiceSizer,  0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
	SetSizer(mainSizer);
	this->Fit();
	Center();
}

void CMemcardSelectPanel::Update()
{
	std::string path = std::string(MemcardPath->GetPath().mb_str());
	if (File::Exists(path) && !File::IsDirectory(path))
	{
		GCMemcard memcard(path.c_str());
		if (memcard.IsValid())
		{
			if (!memcard.IsAsciiEncoding())
				regionChoice->SetSelection(2);
			else
				regionChoice->SetSelection(0);
			regionChoice->Disable();
			u16 size = memcard.GetSize();
			int selection = 0;
			size >>= 2;
			while (size && !(size&1))
			{
				selection++;
				size >>=1;
			}
			blocksChoice->SetSelection(selection);
			blocksChoice->Enable(m_resizeOnly);
			if (!m_resizeOnly)
			{
				createOpenButton->SetId(wxID_IGNORE);
				createOpenButton->SetLabel(_("Open"));
			}
			else
			{
				createOpenButton->SetId(wxID_OK);
				createOpenButton->SetLabel(_("Resize"));
			}

			MemcardPath->Enable(!m_resizeOnly);
			return;
		}
	}
	regionChoice->SetSelection(0);
	regionChoice->Enable(!m_resizeOnly);
	blocksChoice->SetSelection(5);
	blocksChoice->Enable();
	createOpenButton->SetId(wxID_OK);
	createOpenButton->SetLabel(_("Create"));
}
