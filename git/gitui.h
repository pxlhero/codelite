//////////////////////////////////////////////////////////////////////
// This file was auto-generated by codelite's wxCrafter Plugin
// Do not modify this file by hand!
//////////////////////////////////////////////////////////////////////

#ifndef GITUI_BASE_CLASSES_H
#define GITUI_BASE_CLASSES_H

#include <wx/settings.h>
#include <wx/xrc/xmlres.h>
#include <wx/xrc/xh_bmp.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/clrpicker.h>
#include <wx/filepicker.h>
#include <wx/statline.h>
#include <wx/button.h>
#include <wx/splitter.h>
#include <wx/panel.h>
#include <wx/checklst.h>
#include "gitCommitEditor.h"
#include <wx/textctrl.h>
#include <wx/statbox.h>
#include <wx/listctrl.h>
#include <wx/listbox.h>
#include <wx/imaglist.h>
#include <wx/bitmap.h>
#include <map>

class GitSettingsDlgBase : public wxDialog
{
protected:
    wxStaticText* m_staticText1;
    wxColourPickerCtrl* m_colourTrackedFile;
    wxStaticText* m_staticText2;
    wxColourPickerCtrl* m_colourDiffFile;
    wxStaticText* m_staticText4;
    wxFilePickerCtrl* m_pathGIT;
    wxStaticText* m_staticText5;
    wxFilePickerCtrl* m_pathGITK;
    wxStaticLine* m_staticline1;
    wxButton* m_buttonOk;
    wxButton* m_buttonCancel;

protected:

public:
    GitSettingsDlgBase(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Git settings..."), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(-1, -1), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER);
    virtual ~GitSettingsDlgBase();
};


class GitCommitDlgBase : public wxDialog
{
protected:
    wxSplitterWindow* m_splitter2;
    wxPanel* m_panel3;
    wxSplitterWindow* m_splitter1;
    wxPanel* m_panel1;
    wxStaticText* m_staticText6;
    wxCheckListBox* m_listBox;
    wxPanel* m_panel2;
    wxStaticText* m_staticText7;
    GitCommitEditor* m_editor;
    wxPanel* m_panel4;
    wxStaticText* m_staticText8;
    wxTextCtrl* m_commitMessage;
    wxButton* m_button5;
    wxButton* m_button6;

protected:

public:
    GitCommitDlgBase(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Git commit..."), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(-1,-1), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER);
    virtual ~GitCommitDlgBase();
};


class GitCommitListDlgBase : public wxDialog
{
protected:
    wxListCtrl* m_commitListBox;
    wxListBox* m_fileListBox;
    GitCommitEditor* m_editor;
    wxTextCtrl* m_commitMessage;
    wxStaticLine* m_staticline4;
    wxStdDialogButtonSizer* m_sdbSizer1;
    wxButton* m_button136;

protected:
    virtual void OnChangeCommit(wxListEvent& event) { event.Skip(); }
    virtual void OnChangeFile(wxCommandEvent& event) { event.Skip(); }

public:
    GitCommitListDlgBase(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Commit List"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(879,600), long style = wxCAPTION|wxRESIZE_BORDER|wxMAXIMIZE_BOX|wxMINIMIZE_BOX|wxCLOSE_BOX);
    virtual ~GitCommitListDlgBase();
};


class GitDiffDlgBase : public wxDialog
{
protected:
    wxListBox* m_fileListBox;
    GitCommitEditor* m_editor;
    wxStaticLine* m_staticline4;
    wxStdDialogButtonSizer* m_sdbSizer1;
    wxButton* m_button145;

protected:
    virtual void OnChangeFile(wxCommandEvent& event) { event.Skip(); }

public:
    GitDiffDlgBase(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Current Diffs"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(879,600), long style = wxCAPTION|wxRESIZE_BORDER|wxMAXIMIZE_BOX|wxMINIMIZE_BOX|wxCLOSE_BOX);
    virtual ~GitDiffDlgBase();
};


class GitImages : public wxImageList
{
protected:
    // Maintain a map of all bitmaps representd by their name
    std::map<wxString, wxBitmap> m_bitmaps;


protected:

public:
    GitImages();
    const wxBitmap& Bitmap(const wxString &name) const {
        if ( !m_bitmaps.count(name) )
            return wxNullBitmap;
        return m_bitmaps.find(name)->second;
    }
    virtual ~GitImages();
};

#endif
