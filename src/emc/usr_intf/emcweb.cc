/********************************************************************
* Description: emcweb.cc
*   miniemc2 WEB-server's main module
*
*   this file was originally named main.cpp
*
* Author: Sergey U. Kaydalov
* License: GPL Version 2 or later
* System: Linux
*
* Copyright (c) 2012 All rights reserved.
*
* Last change:
* $Revision: 1.1 $
* $Author: GP Orcullo $
* $Date: 2013/01/03 $
********************************************************************/

///////////////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include <signal.h>
#include <dirent.h>
#include <mcheck.h>
#include "rcs.hh"
#include "posemath.h"		// PM_POSE, TO_RAD
#include "emc.hh"		// EMC NML
#include "canon.hh"		// CANON_UNITS, CANON_UNITS_INCHES,MM,CM
#include "emcglb.h"		// EMC_NMLFILE, TRAJ_MAX_VELOCITY, etc.
#include "emccfg.h"		// DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.hh"		// INIFILE
#include "rcs_print.hh"
#include "timer.hh"             // etime()
#include "shcom.hh"             // NML Messaging functions
#include "emcweb/mongoose_wrapper.hh"

#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <queue>
#include <string>
#include "emccontroller.hh"

#define NC_FILES_DIR		__DIR__"/nc_files"
#define DYNAMIC_JSON		"/data/emc_dynamic.json"
#define CONFIG_JSON		"/data/emc_config.json"
#define WWWROOT			__DIR__"/www"
#define WWWPORT			"8080"

typedef std::pair<std::string,std::string>Cmd;
typedef std::queue<Cmd> CmdQueue;
static CmdQueue cmds;
static bool bFinish = false;
static int JogCounter = -1;
static std::string emcCoord;
static std::string ncFilesDir(NC_FILES_DIR);

void ChargeJogCounter()
{
	JogCounter = 2;
}

void AddCmd(const Cmd &command)
{
	cmds.push(command);
}

const Cmd &GetNextCmd()
{
	return cmds.front();
}

const size_t GetCmdQueueSize()
{
	return cmds.size();
}

void DetachLastCmd()
{
	cmds.pop();
}

std::string GetDirectoryListHtml(const char *dir)
{
	std::stringstream rc;
	std::string root=ncFilesDir + "/";
	//Try to open specified folder
	DIR *dp;
	struct dirent *ep;
	root += dir;
	dp = opendir(root.c_str());
	if (dp != NULL) {
		//Directory exists
		rc << "<ul class=\"jqueryFileTree\" style=\"display: none;\">\n";
		while ((ep = readdir(dp)) != NULL) {
			if (::strcmp(ep->d_name, ".") == 0
			    || ::strcmp(ep->d_name, "..") == 0)
				continue;
			//Got next file, check is it directory
			std::string ndir = root + "/" + ep->d_name;
			DIR *d2 = opendir(ndir.c_str());
			if (d2 != NULL) {
				//It's a directory
				rc << "<li class=\"directory collapsed\"><a href=\"#\" rel=\"" << dir  << ep->d_name << "/\">" << ep->d_name << "</a></li>\n";
			} else {
				rc << "<li class=\"file ext_txt\"><a href=\"#\" rel=\"" << dir << ep->d_name << "\">" << ep->d_name << "</a></li>\n";
			}
		}
		rc << "</ul>\n";
	}
	return rc.str();
}


class WebPage
{
public:
	virtual void Init() {};
	virtual void Exit() {};
	virtual bool HandleReq(mongoose::web_response &response, mongoose::web_request &request);
};


bool WebPage::HandleReq(mongoose::web_response &response, mongoose::web_request &request)
{
	miniemc::EmcController *pCtrl = miniemc::EmcController::Instance();
	std::string data = "";
	//Check does EMC2 run and start it if neccecery
	if (pCtrl->GetInitState() == miniemc::EmcController::lsUnloaded
	    || pCtrl->GetInitState() == miniemc::EmcController::lsFault)
		pCtrl->Init(emcCoord.c_str());
	//Check again to be sure is it started
	if (pCtrl->GetInitState() != miniemc::EmcController::lsLoaded) {
		std::cout << "Unable to start EMC2 or starting is in progress" << std::endl;
		data = "Unable to start EMC2 or starting is in progress";
	} else {
		//retriving a command(s) and putting it to cmd's queue
		mongoose::queries query = request.get_queries();
		mongoose::queries::iterator it = query.begin();
		//iterating over queries
		for (; it != query.end(); it++) {
			//Looks for jog_repeat cmd. It's not need to add it to queue
			// We have to just reset joggin alarm timeout
			if (it->first == "jog_repeat") {
				//Handle it
				ChargeJogCounter();
			} else if (it->first == "get_program") {
				data = "{\n\"program\":[";
				miniemc::ListPrt list = pCtrl->GetActualProgramContext();
				miniemc::StringList::iterator it=list->begin();
				for (; it != list->end(); it++) {
					data += "\"" + *it + "\",";
				}
				if (data[data.length()-1] == ',')
					data[data.length()-1] = ' ';
				data += "]\n}\n";

			} else if (it->first == "dir") {
				data = GetDirectoryListHtml(it->second.c_str());
			} else if (it->first == "_") {
				// Do nothing - it's jQuery's thing
			} else {
				AddCmd(*it);
			}
		}

	}
	response.set_status_line("1.1", 200, "OK");
	response.add_header("Connection", "keep-alive");
	response.add_header("Content-Length", data.size());
	response.add_header("Content-Type", "text/plain");
	response.write(data.data(), data.size());
	return true;
};


class ReqHandler
{
public:
	ReqHandler()
		: m_PageList()
		, m_Inited(false) {};

	void Init();
	void Exit();
	bool operator()(mongoose::event_type type, mongoose::tcp_connection connection, mongoose::web_request request);

private:

	void AddPage(const std::string &req, WebPage *page) {
		m_PageList.insert(std::pair< std::string, WebPage * >(req, page));
	}

	std::map<std::string, WebPage * > m_PageList;
	bool m_Inited;
};

void ReqHandler::Init()
{
	if (m_Inited)
		return;
	AddPage("/send_cmd", new WebPage());

	std::map<std::string, WebPage * >::iterator it = m_PageList.begin();
	for (; it != m_PageList.end(); it++) {
		it->second->Init();
	}
	m_Inited = true;
}

void ReqHandler::Exit()
{
	if (!m_Inited)
		return;

	std::map<std::string, WebPage * >::iterator it = m_PageList.begin();
	for (; it != m_PageList.end(); it++) {
		it->second->Exit();
		delete it->second;
	}
	m_Inited = false;
}


bool ReqHandler::operator()(mongoose::event_type type, mongoose::tcp_connection connection, mongoose::web_request request)
{
	std::string uri = request.get_uri();
	std::string log_message = request.get_log_message();

	if (type == mongoose::event_type::event_log) {
	} else if (type == mongoose::event_type::new_request) {
		std::map<std::string, WebPage * >::iterator it = m_PageList.begin();
		for (; it != m_PageList.end(); it++) {
			if (boost::starts_with(uri, it->first)) {
				try {
					mongoose::web_response response(connection);

					bool rc = it->second->HandleReq(response, request);
					if (rc)
						response.send();

					return rc;
				} catch (std::exception &ex) {
					std::cerr << ex.what() << std::endl;
				}
			}
		}
	}

	return false;
}

void GenerateJson(const char *filename)
{
	char tmp[256];
	static char *buff = NULL;

	miniemc::EmcController *pCtrl = miniemc::EmcController::Instance();
	if (pCtrl->GetInitState() == miniemc::EmcController::lsLoaded) {
		if (!buff)
			buff = (char *) malloc(32768);
		strcpy(buff, "{\n \"positions\":[");
		/*
		 * Append Positions
		 */
		for (int i = 0; i <  pCtrl->GetAxisNumber(); i++) {
			char name = pCtrl->GetAxisName(i);
			sprintf(buff+strlen(buff), "%.4f", pCtrl->GetAxisPosition(name));
			if (i != pCtrl->GetAxisNumber() -1)
				strcat(buff, ",");
		}
		/*
		 * Append feedrate
		 */

		strcat(buff, "],\n\"feed\":");
		sprintf(buff+ strlen(buff), "%.2f,\n", pCtrl->GetVelocity());
		/*
		 * Append Relative/Absolute position type
		 */
		sprintf(buff+ strlen(buff), "\"pos_abs\":%s,\n", pCtrl->IsPositionAbsolute() ? "true" : "false");
		/*
		 * Append Limits overrided or not flag
		 */
		sprintf(buff+ strlen(buff), "\"limit_over\":%s,\n", pCtrl->IsLimitsOverrided() ? "true" : "false");
		// Active modal G-code
		memset(tmp, 0 , sizeof(tmp));
		pCtrl->GetActiveCodes(tmp);
		sprintf(buff+ strlen(buff), "\"active_codes\": \"%s\",\n", tmp);
		//Current offsets
		//miniemc::Coord coord = pCtrl->GetCurrentOffset();
		miniemc::Coord coord = pCtrl->GetCurrentOrigin();
		sprintf(buff+ strlen(buff), "\"offsets\":[%.3f,%.3f,%.3f,%.3f,%.3f,%.3f],\n", coord.GetPosIndex(0),  coord.GetPosIndex(1),
		        coord.GetPosIndex(2), coord.GetPosIndex(3), coord.GetPosIndex(4), coord.GetPosIndex(5));
		//Controller state, see enum miniemc::EmcController::ecState
		sprintf(buff+ strlen(buff), "\"ctrl_state\":%d,\n", pCtrl->GetControllerState());
		// Controller mode , see miniemc::EmcController::eCtlMode
		sprintf(buff+ strlen(buff), "\"ctrl_mode\":%d,\n", pCtrl->GetControllerMode());
		// Interpretter state
		sprintf(buff+ strlen(buff), "\"interp_state\":%d,\n", pCtrl->GetInterpState());
		// Is machine in position ?
		sprintf(buff+ strlen(buff), "\"inpos\":%s,\n", pCtrl->IsInpos() ? "true" : "false");
		// Is spindle on ?
		sprintf(buff+ strlen(buff), "\"spindle_on\":%s,\n", pCtrl->IsSpindleOn() ? "true" : "false");
		// Get infos
		if (pCtrl->GetErrorCount(miniemc::EmcController::elInfo) > 0) {
			strcat(buff, "\"info\": [");
			while (pCtrl->GetErrorCount(miniemc::EmcController::elInfo)) {
				sprintf(buff+ strlen(buff), "\"%s\"", pCtrl->GetNextError(miniemc::EmcController::elInfo).c_str());
				if (pCtrl->GetErrorCount(miniemc::EmcController::elInfo)) {
					strcat(buff, ",");
				}
			}
			strcat(buff, "],\n");
		}

		// Get warnings
		if (pCtrl->GetErrorCount(miniemc::EmcController::elWarning) > 0) {
			strcat(buff, "\"warn\": [");
			while (pCtrl->GetErrorCount(miniemc::EmcController::elWarning)) {
				sprintf(buff+ strlen(buff), "\"%s\"", pCtrl->GetNextError(miniemc::EmcController::elWarning).c_str());
				if (pCtrl->GetErrorCount(miniemc::EmcController::elWarning)) {
					strcat(buff, ",");
				}
			}
			strcat(buff, "],\n");
		}

		// Get errorss
		if (pCtrl->GetErrorCount(miniemc::EmcController::elError) > 0) {
			strcat(buff, "\"errors\": [");
			while (pCtrl->GetErrorCount(miniemc::EmcController::elError)) {
				sprintf(buff+ strlen(buff), "\"%s\"", pCtrl->GetNextError(miniemc::EmcController::elError).c_str());
				if (pCtrl->GetErrorCount(miniemc::EmcController::elError)) {
					strcat(buff, ",");
				}
			}
			strcat(buff, "],\n");
		}
		sprintf(buff+ strlen(buff), "\"last_program\":\"%s\",\n", pCtrl->GetLastFileName());
		// Get num lines per context
		sprintf(buff+ strlen(buff), "\"context_size\":%d,\n", pCtrl->GetNumLinesPerContext());
		// Get current context index
		sprintf(buff+ strlen(buff), "\"context_index\":%d,\n", pCtrl->GetActualContextIndex());
		// Offset with actual context
		sprintf(buff+ strlen(buff), "\"context_offset\":%d,\n", pCtrl->GetCurrPosInContext());
		// Context changed flag
		sprintf(buff+ strlen(buff), "\"context_changed\":%s\n", pCtrl->isContextChanged() ? "true" : "false");
		strcat(buff, "}");

		/*
		 * Write to disk
		 */
		int fd = open(filename, O_WRONLY | O_TRUNC | O_SYNC | O_CREAT, 0666);
		if (fd >= 0) {
			write(fd, buff, strlen(buff));
			fsync(fd);
			close(fd);
		} else {
			fprintf(stderr, "can't open DYNAMIC_JSON for write!");
		}
	}
}

void DoCmd()
{
	static int LastJoggingAxis = 0;

	miniemc::EmcController *pCtrl = miniemc::EmcController::Instance();

	if (JogCounter == 0) {
		pCtrl->SendJogStop(LastJoggingAxis);
		JogCounter = -1;
	} else if (JogCounter > 0) JogCounter--;

	while (GetCmdQueueSize() != 0) {

		Cmd cmd = GetNextCmd();
		if (cmd.first == "power") {
			if (cmd.second == "on") {
				pCtrl->MachineOn();
			} else if (cmd.second == "off") {
				pCtrl->MachineOff();
			}
		} else if (cmd.first == "jog") {
			char axis = cmd.second[0];
			float dist = ::atof(&cmd.second.c_str()[1]);
			if (dist != 0) {
				pCtrl->SendJogIncr(pCtrl->AxisToIndex(axis), 1000.0*60.0 , dist);
			} else {
				LastJoggingAxis = pCtrl->AxisToIndex(axis);
				pCtrl->SendJogCont(LastJoggingAxis, cmd.second[1] == '-' ? - 1000.0*60.0 : 1000.0*60.0);
				ChargeJogCounter();
			}
		} else if (cmd.first == "spindle") {
			if (cmd.second == "toogle") {
				if (pCtrl->IsSpindleOn()) {
					pCtrl->Spindle(false);
				} else {
					pCtrl->Spindle(true);
				}
			}
		} else if (cmd.first == "foverride") {
			pCtrl->SetFeedOverridePercent(atoi(cmd.second.c_str()));
		} else if (cmd.first == "homesearch") {
			pCtrl->DoHome(cmd.second.c_str()[0]);
		} else if (cmd.first == "hactivate") {
			int oi = ::atoi(cmd.second.c_str());
			pCtrl->ActivateFixtureOffset(oi);
		} else if (cmd.first == "offsetload") {
			int oi = ::atoi(cmd.second.c_str());
			pCtrl->SetFixtureOffsetToCurrent(oi);
		} else if (cmd.first == "offsetclr") {
			int oi = ::atoi(cmd.second.c_str());
			pCtrl->ClearFixtureOffset(oi);
		} else if (cmd.first == "posmode") {
			pCtrl->SetPositionAbsolute(!pCtrl->IsPositionAbsolute());
		} else if (cmd.first == "gohome") {
			pCtrl->GoHome();
		} else if (cmd.first == "limitoverride") {
			pCtrl->OverrideLimits(!pCtrl->IsLimitsOverrided());
		} else if (cmd.first == "program_start") {
			int n = ::atoi(cmd.second.c_str());
			pCtrl->RunProgram(n);
		} else if (cmd.first == "program_stop") {
			pCtrl->StopProgram();
		} else if (cmd.first == "program_pause") {
			pCtrl->PauseProgram();
		} else if (cmd.first == "program_resume") {
			pCtrl->ResumeProgram();
		} else if (cmd.first == "program_step") {
			pCtrl->StepProgram();
		} else if (cmd.first == "program_load") {
			std::string file = ncFilesDir + "/" + cmd.second;
			std::cout << "load program " << file << std::endl;
			pCtrl->OpenProgram(file.c_str());
		} else if (cmd.first == "exec_mdi") {
			pCtrl->ExecMDI(cmd.second.c_str());
		} else if (cmd.first == "reload") {
			pCtrl->ShellCmd("/bin/emcwebreload", "");
		} else if (cmd.first == "reboot") {
			pCtrl->ShellCmd("/bin/emcwebreboot", "");
		} else if (cmd.first == "set_ip") {
			pCtrl->ShellCmd("/bin/changeip", cmd.second.c_str());
		} else {
			std::cout << "Undefined option " << cmd.first << "=" << cmd.second << std::endl;
		}
		DetachLastCmd();
	}


}

void term_handler(int i)
{
	std::cout << "EMCWEB caught signal " << i << ", exiting" << std::endl;
	bFinish = true;
}

int main(int argc, char* argv[])
{
	IniFile inifile;
	const char *inistring;
	std::string wwwroot(WWWROOT);
	std::string wwwport(WWWPORT);

	// process command line args
	if (0 != emcGetArgs(argc, argv)) {
		rcs_print_error("error in argument list\n");
		exit(1);
	}

	if (!inifile.Open(emc_inifile)) {
		rcs_print_error("cannot open %s\n", emc_inifile);
		exit(1);
	}
	
	if (NULL != (inistring = inifile.Find("COORDINATES", "TRAJ"))) {
		emcCoord = inistring;
	} else {
		rcs_print_error("cannot find COORDINATES value in TRAJ section\n");
		exit(1);
	}

	if (NULL != (inistring = inifile.Find("ROOT", "EMCWEB"))) {
		wwwroot = inistring;
	}

	if (NULL != (inistring = inifile.Find("PORT", "EMCWEB"))) {
		wwwport = inistring;
	}

	if (NULL != (inistring = inifile.Find("NC_FILES_DIR", "EMCWEB"))) {
		ncFilesDir = inistring;
	}

	std::cout << "Web root: " << wwwroot <<std::endl;
	std::cout << "Listening on port: " << wwwport <<std::endl;
	std::cout << "Using NC_FILES_DIR: " << ncFilesDir <<std::endl;
	
	std::string emcCFGJson = wwwroot + CONFIG_JSON;
	std::string emcDYNJson = wwwroot + DYNAMIC_JSON;

	ReqHandler handle;
	signal(SIGINT, term_handler);
	signal(SIGSEGV, term_handler);
	signal(SIGQUIT, term_handler);
	signal(SIGILL, term_handler);
	signal(SIGBUS, term_handler);
	signal(SIGTERM, term_handler);
	signal(SIGPIPE, term_handler);

	handle.Init();
	miniemc::EmcController *pCtrl = miniemc::EmcController::Instance();
	pCtrl->SetIniFile(emc_inifile);
	pCtrl->Init(emcCoord.c_str());
	pCtrl->CreateConfigJSON(emcCFGJson.c_str());
	try {
		mongoose::options options;
		options.add("document_root", wwwroot);
		options.add("listening_ports", wwwport);
		mongoose::web_server server(options, handle);
		while (!bFinish) {
			if (pCtrl->Update()) {
				GenerateJson(emcDYNJson.c_str());
				DoCmd();
			}
			usleep(250000);
		}
	} catch (std::exception &ex) {
		std::cerr << ex.what() << std::endl;
	}
	handle.Exit();
}

///////////////////////////////////////////////////////////////////////////////
