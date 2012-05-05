//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// copyright            : (C) 2008 by Eran Ifrah
// file name            : custombuildrequest.h
//
// -------------------------------------------------------------------------
// A
//              _____           _      _     _ _
//             /  __ \         | |    | |   (_) |
//             | /  \/ ___   __| | ___| |    _| |_ ___
//             | |    / _ \ / _  |/ _ \ |   | | __/ _ )
//             | \__/\ (_) | (_| |  __/ |___| | ||  __/
//              \____/\___/ \__,_|\___\_____/_|\__\___|
//
//                                                  F i l e
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

#ifndef __custombuildrequest__
#define __custombuildrequest__

#include "shell_command.h"
#include "build_config.h"
#include "codelite_exports.h"

class WXDLLIMPEXP_SDK CustomBuildRequest : public ShellCommand
{
	wxString m_fileName;

protected:
	void DoUpdateCommand(IManager *manager, wxString &cmd, ProjectPtr proj, BuildConfigPtr bldConf, bool isClean);

public:
	CustomBuildRequest(const QueueCommand &buildInfo, const wxString &fileName);
	virtual ~CustomBuildRequest();

public:
	//process the request
	virtual void Process(IManager *manager = NULL);

	//setters/getters
	const wxString &GetProjectName() const { return m_info.GetProject(); }
};
#endif // __custombuildrequest__
