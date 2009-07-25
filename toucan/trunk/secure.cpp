/////////////////////////////////////////////////////////////////////////////////
// Author:      Steven Lamerton
// Copyright:   Copyright (C) 2006-2009 Steven Lamerton
// License:     GNU GPL 2 (See readme for more info)
/////////////////////////////////////////////////////////////////////////////////

#include <wx/file.h>
#include <wx/filefn.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

#include "secure.h"
#include "toucan.h"
#include "rules.h"
#include "variables.h"
#include "basicfunctions.h"
#include "data/securedata.h"
#include "forms/frmmain.h"
#include "forms/frmprogress.h"
#include "controls/vdtc.h"

bool Secure(SecureData data, Rules rules, frmProgress *window){
	wxArrayString arrLocation = data.GetLocations();
	//Iterate through the entries in the array
	for(unsigned int i = 0; i < arrLocation.Count(); i++)
	{
		if(wxGetApp().GetAbort()){
			return true;
		}
		data.SetStartLength(arrLocation.Item(i).Length() + 1);
		//Need to add normalisation to SecureData
		if(arrLocation.Item(i) != wxEmptyString){
			if(wxDirExists(arrLocation.Item(i))){
				CryptDir(arrLocation.Item(i), data, rules);
			}
			else if(wxFileExists(arrLocation.Item(i))){
				CryptFile(arrLocation.Item(i), data, rules);
			}
		}
	}
	if(wxGetApp().GetUsesGUI()){
		wxGetApp().MainWindow->m_Secure_TreeCtrl->DeleteAllItems();
		wxGetApp().MainWindow->m_Secure_TreeCtrl->AddRoot(wxT("HiddenRoot"));
		for(unsigned int i = 0; i < wxGetApp().MainWindow->m_SecureLocations->GetCount(); i++){
			wxGetApp().MainWindow->m_Secure_TreeCtrl->AddNewPath(Normalise(Normalise(wxGetApp().MainWindow->m_SecureLocations->Item(i))));
		}
	}
	wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, ID_SCRIPTFINISH);
	wxPostEvent(window, event);	
	return true;
}


/*The main loop for the Secure process. It is called by Secure initially and then either calls itself when it reaches a
folder or CryptFile when it reaches a file.*/
bool CryptDir(wxString strPath, SecureData data, Rules rules)
{   
	if(wxGetApp().GetAbort()){
		return true;
	}
	wxDir dir(strPath);
	wxString filename;
	bool blDir = dir.GetFirst(&filename);
	if (blDir)
	{
		do {
			if(wxGetApp().GetAbort()){
				return true;
			}
			if (wxDirExists(strPath + wxFILE_SEP_PATH + filename) ){
				CryptDir(strPath + wxFILE_SEP_PATH + filename, data, rules);
			}
			else{
				CryptFile(strPath + wxFILE_SEP_PATH + filename, data, rules);
			}
		}
		while (dir.GetNext(&filename) );
	}   
	return true;
}


bool CryptFile(wxString strFile, SecureData data, Rules rules)
{
	if(wxGetApp().GetAbort()){
		return true;
	}
	//Check to see it the file should be excluded	
	if(rules.ShouldExclude(strFile, false)){
		return true;
	}
	//Make sure that it is a 'real' file
	wxFileName filename(strFile);
	if(filename.IsOk() == true){
		wxString size = filename.GetHumanReadableSize();
		if(size == wxT("Not available")){
			return false;
		}
	}

	//Ensure that we are not encrypting an already encrypted file or decrypting a non encrypted file
	if(filename.GetExt() == wxT("cpt") && data.GetFunction() == _("Encrypt")){
		return false;
	}
	if(filename.GetExt() != wxT("cpt") && data.GetFunction() == _("Decrypt")){
		return false;
	}	
	
	//A couple of arrays to catch the output and surpress the command window
	wxArrayString arrErrors;
	wxArrayString arrOutput;

	if(data.GetFunction() == _("Encrypt")){
		wxString exe;
		//Set the file to have normal attributes so it can be encrypted
		#ifdef __WXMSW__   
			exe = wxT("ccrypt.exe");
			int filearrtibs = GetFileAttributes(strFile);
			SetFileAttributes(strFile,FILE_ATTRIBUTE_NORMAL);  
		#else
			exe = wxT("./ccrypt");
		#endif

		wxSetWorkingDirectory(wxPathOnly(wxStandardPaths::Get().GetExecutablePath()));

		//Create and execute the command
		wxString command = exe + wxT(" -e -K\"") + data.GetPassword() + wxT("\" \"") + strFile + wxT("\"");
		long lgReturn = wxExecute(command, arrErrors, arrOutput, wxEXEC_SYNC|wxEXEC_NODISABLE);

		//Put the old attributed back
		#ifdef __WXMSW__
			SetFileAttributes(strFile,filearrtibs);
		#endif

		if(lgReturn == 0){        
			OutputProgress(_("Encrypted ") + strFile.Right(strFile.Length() - data.GetStartLength()));
		}
		else{
			OutputProgress(_("Failed to encrypt ") + strFile.Right(strFile.Length() - data.GetStartLength()) + wxString::Format(wxT(" : %i"), lgReturn), wxDateTime::Now().FormatTime(), true);
		}
	}

	else if(data.GetFunction() == _("Decrypt")){
		wxString exe;
		//Set the file to have normal attributes so it can be decrypted
		#ifdef __WXMSW__
			exe = wxT("ccrypt.exe");
			int filearrtibs = GetFileAttributes(strFile);
			SetFileAttributes(strFile,FILE_ATTRIBUTE_NORMAL);
		#else
			exe = wxT("./ccrypt");
		#endif
		
		if(wxFileExists(strFile.Left(strFile.Length() - 4))){
			//We have a file with the decryped name already there, skip it
			OutputProgress(_("Failed to decrypt ") + strFile.Right(strFile.Length() - data.GetStartLength()), wxDateTime::Now().FormatTime(), true);
			return true;
		}

		wxSetWorkingDirectory(wxPathOnly(wxStandardPaths::Get().GetExecutablePath()));

		//Create and execute the command
		wxString command = exe + wxT(" -f -d -K\"") + data.GetPassword() + wxT("\" \"") + strFile + wxT("\"");
		long lgReturn = wxExecute(command, arrErrors, arrOutput, wxEXEC_SYNC|wxEXEC_NODISABLE);

		//Put the old attributed back
		#ifdef __WXMSW__
			SetFileAttributes(strFile,filearrtibs);
		#endif

		if(lgReturn == 0){       
 			OutputProgress(_("Decrypted ") + strFile.Right(strFile.Length() - data.GetStartLength()));
		}
		else{
 			OutputProgress(_("Failed to decrypt ") + strFile.Right(strFile.Length() - data.GetStartLength()) + wxString::Format(wxT(" : %i"), lgReturn), wxDateTime::Now().FormatTime(), true);
		}
	}
	IncrementGauge();
	return true;
}
