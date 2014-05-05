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
#include "MCMmain.h"
#include "Sram.h"
IMPLEMENT_APP(MCMApp)

CMemcardManager *main_frame;

#if defined HAVE_WX && HAVE_WX 
bool wxMsgAlert(const char* caption, const char* text, bool yes_no, int /*Style*/) 
{
	return wxYES == wxMessageBox(wxString::FromAscii(text), 
				 wxString::FromAscii(caption),
				 (yes_no)?wxYES_NO:wxOK);
}
#endif

bool MCMApp::OnInit()
{
	// Register message box handler
	#if defined(HAVE_WX) && HAVE_WX
		RegisterMsgAlertHandler(&wxMsgAlert);
	#endif

	main_frame = new CMemcardManager((wxFrame*) NULL, wxID_ANY, wxString::FromAscii("Memcard Manager"),
				wxPoint(100, 100), wxSize(800, 600));
	main_frame->Show();
	SetTopWindow(main_frame);
	return true;
}
namespace CEXIIPL
{
u32 GetGCTime()
{
	const u32 cJanuary2000 = 0x386D42C0;  // Seconds between 1.1.1970 and 1.1.2000
	u64 ltime = Common::Timer::GetLocalTimeSinceJan1970();
	return ((u32)ltime - cJanuary2000);
}
};

SRAM g_SRAM = {{
	0x04, 0x6B,
	0xFB, 0x91,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0x40,
	0x05,
	0x00,
	0x00,
	0x2C,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xD2, 0x2B,	0x29, 0xD5,	0xC7, 0xAA,	0x12, 0xCB,	0x21, 0x27,	0xD1, 0x53,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x86,
	0x00,
	0xFF, 0x4A,
	0x00, 0x00,
	0x00, 0x00
}};