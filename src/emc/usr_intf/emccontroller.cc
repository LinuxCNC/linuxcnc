/********************************************************************
* Description: emccontroller.cc
*   miniemc2 controller
*
*   this file was originally named emccontroller.cpp
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


#include "rcs.hh"
#include "posemath.h"		// PM_POSE, TO_RAD
#include "emc.hh"		// EMC NML
#include "canon.hh"		// CANON_UNITS, CANON_UNITS_INCHES,MM,CM
#include "emcglb.h"		// EMC_NMLFILE, TRAJ_MAX_VELOCITY, etc.
#include "emccfg.h"		// DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.hh"		// INIFILE
#include "rcs_print.hh"
#include "timer.hh"		// etime()
#include "shcom.hh"		// NML Messaging functions
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "emccontroller.hh"
#include <string.h>

void InitMain();
int nmlUpdate(const char *inifile);

namespace miniemc
{
const int cNumLinesPerContext = 16;
const char *cAxesNames = "XYZABC";

EmcController::EmcController()
	: m_iniFile()
	, m_loadState(lsUnloaded)
	, m_positionAbsolute(true)
	, m_overrideLimits(false)
	, m_threadExit(false)
{
	m_pthread = new boost::thread(&EmcController::Init_thread , this);

}

void EmcController::SetIniFile(const std::string &filename)
{
	m_iniFile = filename;
}

int  EmcController::GetAxisNumber()
{
	return strlen(cAxesNames);
}

char EmcController::GetAxisName(int i)
{
	if (i < GetAxisNumber()) {
		return cAxesNames[i];
	} else {
		return '-';
	}
}

int  EmcController::AxisToIndex(char c)
{
	for (int i = 0; i < GetAxisNumber(); i++) {
		if (GetAxisName(i) == c)
			return i;
	}
	return -1;
}

bool EmcController::IsAxisUsed(int i)
{
	if ((int) m_used_map.size() > i) {
		return m_used_map[GetAxisName(i)];
	} else {
		return false;
	}
}

void EmcController::CreateConfigJSON(const char *fname)
{
	std::ofstream json(fname, std::ios_base::trunc | std::ios_base::out);
	json << "{\n\"axes\":[\n";
	// Iterate over all possible axes
	for (int i = 0; i < GetAxisNumber(); i++) {
		// Check if axis present in configuration
		if (IsAxisUsed(i)) {
			json << "{\"used\": true,\n";
		} else {
			json << "{\"used\": false,\n";
		}
		json << "\"slave_for\": \"-\",\n";
		json << "\"name\":\"" << GetAxisName(i) << "\"\n}";
		if (i + 1 != GetAxisNumber())
			json << ",";
	}
	json << "]\n}";
	json.close();
}

int EmcController::ShellCmd(const char *cmd, const char *arg)
{
	const char *argv[3] = {cmd, arg, NULL};
	printf("Shell cmd=%s, arg=%s\n",cmd , arg);
	sleep(1);
	pid_t kidpid = vfork();
	if (kidpid < 0) {
		perror("Internal error: cannot fork.");
		return -1;
	} else if (kidpid == 0) {
		// I am the child.
		execvp(cmd, (char* const *)argv);
		// The following lines should not happen (normally).
		perror(cmd);
		return -2;
	} else {
		// I am the parent.  Wait for the child.
		if (waitpid(kidpid, 0, 0) < 0) {
			perror("Internal error: cannot wait for child.");
			return -3;
		}
	}
	return 0;
}

EmcController::~EmcController()
{
	std::cerr << "EmcController::~EmcController()" << std::endl;

	m_threadExit = true;
	m_pthread->join();
	delete m_pthread;
	Exit_internal();
}

EmcController::loadState EmcController::Init(const char *config)
{
	loadState state = GetInitState();
	if (state == lsUnloaded || state == lsFault) {
		// Prepare axis configuration
		m_used_map.clear();
		std::cout << "Config string " << config << std::endl;
		// Iterate over all possible axes
		for (int i = 0; i < GetAxisNumber(); i++) {
			char name = GetAxisName(i);
			bool used = ::strchr(config, name) != NULL;
			m_used_map.insert(std::pair<char,bool>(name, used));
		}

		SetInitState(lsStartProgress);
		while ((state = GetInitState()) == lsStartProgress)
			usleep(100000);
	}
	return state;
}

bool EmcController::InitAsync()
{
	loadState state = GetInitState();
	if (state == lsStartProgress || state == lsStopProgress)
		return false;
	SetInitState(lsStartProgress);
	return true;
}

void EmcController::Exit()
{
	loadState state = GetInitState();
	if (state == lsLoaded) {
		SetInitState(lsStopProgress);
		while (GetInitState() == lsStopProgress)
			usleep(100000);
	}
}

void EmcController::ExitAsync()
{
	loadState state = GetInitState();
	if (state == lsStopProgress || state == lsStartProgress || state == lsFault)
		return;
	SetInitState(lsStopProgress);
}

EmcController::loadState EmcController::GetInitState()
{
	boost::mutex::scoped_lock lock(m_guard);
	return m_loadState;
}

void EmcController::SetInitState(EmcController::loadState mode)
{
	boost::mutex::scoped_lock lock(m_guard);
	m_loadState = mode;
}

void EmcController::Init_thread()
{
	while (m_threadExit == false) {
		if (GetInitState() == lsStartProgress) {
			if (Init_internal() == true)
				SetInitState(lsLoaded);
			else
				SetInitState(lsFault);
		}

		if (GetInitState() == lsStopProgress) {
			Exit_internal();
			SetInitState(lsUnloaded);
		}
		usleep(100000);
	}
}

bool EmcController::Init_internal()
{
	::InitMain();
	::iniLoad(m_iniFile.c_str());
	return nmlUpdate(m_iniFile.c_str()) == 0;
}



void EmcController::Exit_internal()
{
	EMC_NULL emc_null_msg;

	if (emcStatusBuffer != 0) {
		// wait until current message has been received-lboost_thread-mt -lpthread
		emcCommandWaitReceived(emcCommandSerialNumber);
	}

	if (emcCommandBuffer != 0) {
		// send null message to reset serial number to original
		emc_null_msg.serial_number = saveEmcCommandSerialNumber;
		emcCommandBuffer->write(emc_null_msg);
	}

	// clean up NML buffers
	if (emcErrorBuffer != 0) {
		delete emcErrorBuffer;
		emcErrorBuffer = 0;
	}

	if (emcStatusBuffer != 0) {
		delete emcStatusBuffer;
		emcStatusBuffer = 0;
		emcStatus = 0;
	}

	if (emcCommandBuffer != 0) {
		delete emcCommandBuffer;
		emcCommandBuffer = 0;
	}
}


bool EmcController::Update()
{
	if (GetInitState() != lsLoaded)
		return false;
	int rc;
	if ((rc = nmlUpdate(m_iniFile.c_str())) == 0) {
		if (error_string[0] != 0) {
			AddErrorString(elError, error_string);
			error_string[0] = 0;
		}
		if (operator_text_string[0] != 0) {
			AddErrorString(elWarning, operator_text_string);
			operator_text_string[0] = 0;
		}
		if (operator_display_string[0] != 0) {
			AddErrorString(elWarning, operator_display_string);
			operator_display_string[0] = 0;
		}
	}
	return rc == 0;
}


int EmcController::GetErrorCount(eLogType lType)
{
	return m_Log[lType].size();
}

std::string EmcController::GetNextError(eLogType lType)
{
	std::string rc = "";
	if (GetErrorCount(lType) > 0) {
		rc = m_Log[lType].front();
		m_Log[lType].pop();
	}
	return rc;
}

void EmcController::AddErrorString(eLogType lType, const char *error)
{
	if (error && ::strlen(error) > 0) {
		m_Log[lType].push(std::string(error));
	}
}

EmcController::ecState EmcController::GetControllerState()
{
	if (GetInitState() != lsLoaded)
		return ecOff;

	return (ecState)emcStatus->task.state;
}

/*
 *  Controller MODES mamangement (get, set)
 */
EmcController::eCtlMode EmcController::GetControllerMode()
{
	if (GetInitState() != lsLoaded)
		return emManual;
	return (eCtlMode) emcStatus->task.mode;
}


bool EmcController::SetControllerMode(eCtlMode mode)
{
	if (GetInitState() != lsLoaded)
		return false;

	bool rc = true;
	if (GetControllerMode() != mode) {
		if (IsInpos()) {
			switch (mode) {
			case emManual:
				::sendManual();
				break;

			case emAuto:
				::sendAuto();
				break;

			case emMDI:
				::sendMdi();
				break;

			default:
				break;
			}
		} else {
			AddErrorString(elWarning , "Machine is busy!");
			rc = false;
		}
	}
	return rc;
}


EmcController::eInterpState EmcController::GetInterpState()
{
	if (GetInitState() != lsLoaded)
		return eiIdle;
	return (eInterpState) emcStatus->task.interpState;
}

bool EmcController::IsInpos()
{
	if (GetInitState() != lsLoaded)
		return true;
	return emcStatus->motion.traj.inpos != 0;
}

void EmcController::MachineOn()
{
	Coord cord; // Zero offset
	if (GetInitState() != lsLoaded)
		return;
	sendMachineOn();
}

void EmcController::MachineOff()
{
	if (GetInitState() != lsLoaded)
		return;
	sendMachineOff();
}

Coord EmcController::GetCurrentOrigin()
{
	Coord cord;
	cord.SetPosByIndex(0, (emcStatus->task.g5x_offset.tran.x + emcStatus->task.g92_offset.tran.x));
	cord.SetPosByIndex(1, (emcStatus->task.g5x_offset.tran.y + emcStatus->task.g92_offset.tran.y));
	cord.SetPosByIndex(2, (emcStatus->task.g5x_offset.tran.z + emcStatus->task.g92_offset.tran.z));
	cord.SetPosByIndex(3, (emcStatus->task.g5x_offset.a + emcStatus->task.g92_offset.a));
	cord.SetPosByIndex(4, (emcStatus->task.g5x_offset.b + emcStatus->task.g92_offset.b));
	cord.SetPosByIndex(5, (emcStatus->task.g5x_offset.c + emcStatus->task.g92_offset.c));
	return cord;
}

double EmcController::GetAxisPosition(char axis)
{
	double rc = 0;
	if (GetInitState() != lsLoaded)
		return 0;
	switch (axis) {
	case 'X':
		rc = emcStatus->motion.traj.position.tran.x;
		if (!IsPositionAbsolute())
			rc -= (emcStatus->task.g5x_offset.tran.x + emcStatus->task.g92_offset.tran.x);
		break;

	case 'Y':
		rc = emcStatus->motion.traj.position.tran.y;
		if (!IsPositionAbsolute())
			rc -= (emcStatus->task.g5x_offset.tran.y + emcStatus->task.g92_offset.tran.y);
		break;

	case 'Z':
		rc = emcStatus->motion.traj.position.tran.z;
		if (!IsPositionAbsolute())
			rc -= (emcStatus->task.g5x_offset.tran.z + emcStatus->task.g92_offset.tran.z);
		break;

	case 'A':
		rc = emcStatus->motion.traj.position.a;
		if (!IsPositionAbsolute())
			rc -= (emcStatus->task.g5x_offset.a + emcStatus->task.g92_offset.a);
		break;

	case 'B':
		rc = emcStatus->motion.traj.position.b;
		if (!IsPositionAbsolute())
			rc -= (emcStatus->task.g5x_offset.b + emcStatus->task.g92_offset.b);
		break;

	case 'C':
		rc = emcStatus->motion.traj.position.c;
		if (!IsPositionAbsolute())
			rc -= (emcStatus->task.g5x_offset.c + emcStatus->task.g92_offset.c);
		break;

	default:
		break;
	}
	return rc;
}

double EmcController::GetVelocity()
{
	if (GetInitState() != lsLoaded)
		return 0;
	return emcStatus->motion.traj.current_vel;
}


void EmcController::Spindle(bool state)
{
	if (GetInitState() != lsLoaded)
		return;

	if (state)
		::sendSpindleForward();
	else
		::sendSpindleOff();
}

bool EmcController::IsSpindleOn()
{
	if (GetInitState() != lsLoaded)
		return false;

	if (emcStatus->motion.spindle.increasing > 0
	    || emcStatus->motion.spindle.increasing < 0
	    || emcStatus->motion.spindle.direction > 0
	    || emcStatus->motion.spindle.direction < 0)
		return true;
	else
		return false;
}

int EmcController::SendJogCont(int axis, double speed)
{
	if (GetInitState() != lsLoaded)
		return -1;
	int rc = -1;
	if (SetControllerMode(emManual)) {
		rc = ::sendJogCont(axis, speed);
	}
	return rc;
}

int EmcController::SendJogIncr(int axis, double speed, double incr)
{
	if (GetInitState() != lsLoaded)
		return -1;
	int rc = -1;
	if (SetControllerMode(emManual)) {
		rc = ::sendJogIncr(axis, speed, incr);
	}
	return rc;
}

int EmcController::SendJogStop(int axis)
{
	if (GetInitState() != lsLoaded)
		return -1;
	int rc = ::sendJogStop(axis);
	return rc;
}

double EmcController::GetMaxVelocity()
{
	if (GetInitState() != lsLoaded)
		return 0;
	double rc = emcStatus->motion.traj.maxVelocity;
	return rc * 60.0;
}

void EmcController::SetFeedOverridePercent(int val)
{
	if (GetInitState() != lsLoaded)
		return;

	double ovr = (double)val/100.0;
	::sendFeedOverride(ovr);
}

bool EmcController::IsMachineFree()
{
	if (GetInitState() != lsLoaded)
		return false;

	if (GetControllerState() != ecOn) {
		AddErrorString(elWarning, "Machine is OFF!");
		return false;
	}
	if (!IsInpos()) {
		AddErrorString(elWarning, "Machine is moving!");
		return false;
	}
	return  true;
}

void EmcController::DoHome(char axis)
{
	if (GetInitState() != lsLoaded)
		return;

	if (GetControllerState() == ecOn) {
		SetControllerMode(emManual);
		int idx = AxisToIndex(axis);
		::sendHome(idx);
	}
}

bool EmcController::ExecMDI(const char *cmd)
{
	bool rc = false;

	if (GetInitState() != lsLoaded)
		return rc;

	if (GetControllerState() == ecOn && (GetInterpState() == eiPaused || GetInterpState() == eiIdle)) {
		SetControllerMode(emMDI);
		::sendMdiCmd(const_cast<char *>(cmd));
		std::cerr << "ExecMDI:" << cmd << std::endl;
		rc = true;
	}
	return rc;
}

void EmcController::SetFixtureOffset(const Coord &coord, int index)
{
	char buff[256];
	if (GetInitState() != lsLoaded)
		return;

	if (IsMachineFree()) {
		if (index >=0 && index < 8) {
			::sprintf(buff, "G10 L2 P%1d", index+1);
			for (int i=0; i < GetAxisNumber(); i++) {
				if (IsAxisUsed(i)) {
					::sprintf(buff + ::strlen(buff), " %c%.4f"
					        , GetAxisName(i)
					        , coord.GetPosIndex(i));
				}
			}
			ExecMDI(buff);
		}
	}
}

void EmcController::SetFixtureOffsetToCurrent(int index)
{
	if (GetInitState() != lsLoaded)
		return;

	double crd[6];
	bool oldPos = IsPositionAbsolute();

	SetPositionAbsolute(true);
	for (int i = 0; i <GetAxisNumber(); i++) {
		crd[i] = GetAxisPosition(GetAxisName(i));
	}
	SetPositionAbsolute(oldPos);

	Coord coord(crd);
	SetFixtureOffset(coord, index);
}

void EmcController::ActivateFixtureOffset(int index)
{
	if (GetInitState() != lsLoaded)
		return;

	static const char *gcode[]= {"G54","G55", "G56", "G57", "G58", "G59", "G59.1", "G59.2"};
	if (IsMachineFree()) {
		ExecMDI(gcode[index]);
	}
}

void EmcController::ClearFixtureOffset(int index)
{
	if (GetInitState() != lsLoaded)
		return;

	SetFixtureOffset(Coord(), index);
}

Coord EmcController::GetCurrentOffset()
{
	Coord offset;
	if (GetInitState() != lsLoaded)
		return offset;

	offset.SetPosByIndex(0, (emcStatus->task.g5x_offset.tran.x + emcStatus->task.g92_offset.tran.x));
	offset.SetPosByIndex(1, (emcStatus->task.g5x_offset.tran.y + emcStatus->task.g92_offset.tran.y));
	offset.SetPosByIndex(2, (emcStatus->task.g5x_offset.tran.z + emcStatus->task.g92_offset.tran.z));
	offset.SetPosByIndex(3, (emcStatus->task.g5x_offset.a + emcStatus->task.g92_offset.a));
	offset.SetPosByIndex(4, (emcStatus->task.g5x_offset.b + emcStatus->task.g92_offset.b));
	offset.SetPosByIndex(5, (emcStatus->task.g5x_offset.c + emcStatus->task.g92_offset.c));
	return offset;
}

void EmcController::GoHome()
{
	if (GetInitState() != lsLoaded)
		return;

	std::stringstream cmd;
	cmd << "G0";
	for (int i=0; i < GetAxisNumber(); i++) {
		if (IsAxisUsed(i)) {
			cmd << " " << GetAxisName(i) << 0;
		}
	}
	ExecMDI(cmd.str().c_str());
}

void EmcController::OverrideLimits(bool val)
{
	if (GetInitState() != lsLoaded)
		return;

	if (val != IsLimitsOverrided()) {
		SetControllerMode(emManual);
		int ov = val == true ? 1 : -1;
		::sendOverrideLimits(ov);
		m_overrideLimits = val;
	}
}

bool EmcController::IsLimitsOverrided()
{
	return emcStatus->motion.axis[0].overrideLimits != 0;
}

void EmcController::GetActiveCodes(char *out_buf)
{
	if (GetInitState() != lsLoaded)
		return;

	int code, i;
	char buf[256] = {0};
	for (i=1; i<ACTIVE_G_CODES; i++) {
		code = emcStatus->task.activeGCodes[i];
		if (code == -1) continue;
		if (code % 10) sprintf(buf, "G%.1f ", (double) code / 10.0);
		else sprintf(buf, "G%d ", code / 10);
		strcat(out_buf, buf);
	}
	sprintf(buf, "F%.0f ", emcStatus->task.activeSettings[1]);
	strcat(out_buf, buf);
	sprintf(buf, "S%.0f", rtapi_fabs(emcStatus->task.activeSettings[2]));
	strcat(out_buf, buf);
}

void EmcController::OpenProgram(const char *name)
{
	if (GetInitState() != lsLoaded)
		return;
	if (GetControllerMode() != emAuto)
		SetControllerMode(emAuto);
	::sendTaskPlanInit();
	::sendProgramOpen(const_cast<char *>(name));
	InitProgramContext();
}

const char *EmcController::GetLastFileName() const
{
	if (emcStatus->task.file[0] != 0)
		return emcStatus->task.file;
	else
		return "";
}

void EmcController::InitProgramContext()
{
	if (m_pgm_file.is_open())
		m_pgm_file.close();
	m_pgm_file.open(GetLastFileName(), std::ios_base::in);
	if (!m_pgm_file.good()) {
		AddErrorString(elError, "Unable to open file for parsing");
	}
	m_last_context_index = -1;
	m_lines_map.clear();
	m_context.clear();
}


bool EmcController::isContextChanged()
{
	return m_bForceUpdate;
}

ListPrt EmcController::GetActualProgramContext()
{
	static boost::mutex l_lock;
	boost::mutex::scoped_lock lock(l_lock);

	if (GetLastFileName()[0] != '\0') {	/* if not empty */
		if (m_bForceUpdate) {
			InitProgramContext();
			m_bForceUpdate = false;
		}

		if (m_last_context_index < 0 || (int) (GetLineNumber() / GetNumLinesPerContext()) != m_last_context_index) {
			/*
			 * Have out of context, load next one
			 */
			m_last_context_index = GetLineNumber() / GetNumLinesPerContext();
			/*
			 * Try to find cached position inside map
			 */
			std::map<int,int>::iterator it = m_lines_map.begin();
			int last_nearest_block = 0, last_nearest_pos = 0;
			for (; it != m_lines_map.end(); it++) {
				int index = it->first;
				if (index > last_nearest_block && index <= m_last_context_index) {
					last_nearest_pos = it->second;
					last_nearest_block = index;
					if (m_last_context_index ==  index)
						break;
				}
			}
			/*
			 * Iterate over a program file till we found actual context
			 * Store context to stringlist
			 */
			m_pgm_file.clear();
			m_pgm_file.seekg(last_nearest_pos, std::ios_base::beg);
			m_context.clear();
			while (last_nearest_block <= m_last_context_index) {
				char tmp[256], tmp2[256];
				try {
					// Dummy read of data block
					for (uint i=0; i < GetNumLinesPerContext(); i++) {
						if (!m_pgm_file.eof()) {
							m_pgm_file.getline(tmp, 256);
							if (m_last_context_index == last_nearest_block) {
								int len = ::strlen(tmp);
								for (int i=0; i < len; i++) {
									bool sleft = false, sright = false;
									if (tmp[i] == '\r' || tmp[i] == '\n')
										sleft = true;
									else if (tmp[i] == '\'' || tmp[i] == '\"')
										sright = true;
									if (sleft) {
										strcpy(&tmp[i], &tmp[i+1]);
									}
									if (sright) {
										char c = tmp[i];
										strcpy(tmp2, &tmp[i+1]);
										tmp[i] = '\\';
										tmp[i+1] = c;
										strcpy(&tmp[i+2], tmp2);
										i++;
									}
								};
								m_context.push_back(tmp);
							}
						}
					}
					// Increment block index
					last_nearest_block++;
					// Cache current block position
					m_lines_map.insert(std::pair<int,int>(last_nearest_block, m_pgm_file.tellg()));
				} catch (std::ifstream::failure e) {
					// Had reading error, exiting with an old or incomplet context
					std::cout << "pgm read error: " << e.what() << std::endl;
					AddErrorString(elError, e.what());
					break;
				}
			}

		}
	}
	return ListPrt(new StringList(m_context));
}

uint32_t EmcController::GetActualContextIndex()
{
	static time_t last_time = 0;
	static std::string last_program;
	struct stat lstat;
	::stat(GetLastFileName(), &lstat);
	if ((last_time != lstat.st_mtime)  || (last_program != GetLastFileName())) {
		last_program = GetLastFileName();
		last_time = lstat.st_mtime;
		m_bForceUpdate = true;
	}

	return GetLineNumber()/GetNumLinesPerContext();
}

uint32_t EmcController::GetCurrPosInContext()
{
	return GetLineNumber()%GetNumLinesPerContext();
}

uint32_t EmcController::GetNumLinesPerContext()
{
	return cNumLinesPerContext;
}

void EmcController::RunProgram(int line)
{
	if (GetInitState() != lsLoaded)
		return;

	if (IsMachineFree() && GetInterpState() == eiIdle) {
		SetControllerMode(emAuto);
		::sendProgramRun(line);
	}
}

void EmcController::PauseProgram()
{
	if (GetInitState() != lsLoaded)
		return;

	::sendProgramPause();
}

void EmcController::ResumeProgram()
{
	if (GetInitState() != lsLoaded)
		return;

	::sendProgramResume();
}

void EmcController::StepProgram()
{
	if (GetInitState() != lsLoaded)
		return;

	::sendProgramStep();
}

void EmcController::StopProgram()
{
	if (GetInitState() != lsLoaded)
		return;
	m_lastProgramFile= GetLastFileName();
	::sendAbort();
	usleep(100000);
	OpenProgram(m_lastProgramFile.c_str());
}

int EmcController::GetLineNumber()
{
	if (GetInitState() != lsLoaded)
		return 0;

	int lineNo;
	if ((programStartLine< 0) || (emcStatus->task.readLine < programStartLine)) {
		lineNo = emcStatus->task.readLine;
	} else if (emcStatus->task.currentLine > 0)
		if ((emcStatus->task.motionLine > 0) &&
		    (emcStatus->task.motionLine < emcStatus->task.currentLine)) {
			lineNo = emcStatus->task.motionLine;
		} else {
			lineNo = emcStatus->task.currentLine;
		}
	else {
		lineNo = 1;
	}
	return lineNo;
}
}

void InitMain()
{
	emcWaitType = EMC_WAIT_RECEIVED;
	emcCommandSerialNumber = 0;
	saveEmcCommandSerialNumber = 0;
	emcTimeout = 0.0;
	emcUpdateType = EMC_UPDATE_AUTO;
	linearUnitConversion = LINEAR_UNITS_AUTO;
	angularUnitConversion = ANGULAR_UNITS_AUTO;
	emcCommandBuffer = 0;
	emcStatusBuffer = 0;
	emcStatus = 0;

	emcErrorBuffer = 0;
	error_string[LINELEN-1] = 0;
	operator_text_string[LINELEN-1] = 0;
	operator_display_string[LINELEN-1] = 0;
	programStartLine = 0;
}

int nmlUpdate(const char *inifile)
{
	static enum { IDLE, TRY, CONNECTED } status = IDLE;
	int rc = -1;
	switch (status) {
	case IDLE:
		status = TRY;
		fprintf(stderr, "Init NML connection\n");
		::InitMain();
		::iniLoad(inifile);

	case TRY:
		// We have no connection at this moment
		fprintf(stderr, "Try to establish NML connection\n");
		if (emcTaskNmlGet() != 0)
			break;
		if (emcErrorNmlGet() != 0)
			break;
		fprintf(stderr, "NML connected\n");
		status = CONNECTED;

	case CONNECTED:
		if (updateStatus() != 0
		    || updateError() != 0) {
			fprintf(stderr, "NML connection losts\n");
			// Connection lost, deleting channels
			if (emcCommandBuffer) {
				delete emcCommandBuffer;
				emcCommandBuffer = 0;
			}

			if (emcStatusBuffer) {
				delete emcStatusBuffer;
				emcStatusBuffer = 0;
				emcStatus = 0;
			}
			status = TRY;
			rc = -1;
		} else {
			emcCommandSerialNumber = emcStatus->echo_serial_number;
			saveEmcCommandSerialNumber = emcStatus->echo_serial_number;
			rc = 0;
		}
		break;

	default:
		break;
	}

	return rc;
}
