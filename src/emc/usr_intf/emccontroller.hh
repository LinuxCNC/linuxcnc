/********************************************************************
* Description: emccontroller.hh
*   miniemc2 controller
*
*   this file was originally named emccontroller.h
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

#ifndef EMCCONTROLLER_HH
#define EMCCONTROLLER_HH

#include <string>
#include <queue>
#include <iostream>
#include <fstream>
#include <map>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include "emcweb/singleton.hh"


namespace miniemc
{
class MiniEmcConfig;
class Coord;
typedef std::list< std::string > StringList ;
typedef boost::shared_ptr<StringList> ListPrt;

struct Coord {
	Coord() {for (int i=0; i < 6; i++) pos[i] = 0;}
	Coord(double npos[]) { for (int i=0; i < 6; i++) pos[i] = npos[i]; }
	Coord(const Coord &cpy) {for (int i=0; i < 6; i++) pos[i] = cpy.pos[i];}
	inline bool operator==(const Coord &left) const {
		return (this->pos[0] == left.pos[0]
		        &&  this->pos[1] == left.pos[1]
		        &&  this->pos[2] == left.pos[2]
		        &&  this->pos[3] == left.pos[3]
		        &&  this->pos[4] == left.pos[4]
		        &&  this->pos[5] == left.pos[5]);
	}

	/* inline double GetPosByAxis(char name) const
	 {
	     static const char* ati = "XYZABC";
	     if(i >=0 || i < 6)
		 return pos[i];
	     else
		 return 0;
	 }*/

	inline double GetPosIndex(int idx) const {
		if (idx >=0 || idx < 6)
			return pos[idx];
		else
			return 0;
	}

	void SetPosByIndex(int idx, double value) {
		pos[idx] = value;
	}

	inline double GetX() const { return pos[0]; }
	inline void SetX(double npos) { pos[0] = npos; }

	inline double GetY() const { return pos[1]; }
	inline void SetY(double npos) { pos[1] = npos; }

	inline double GetZ() const { return pos[2]; }
	inline void SetZ(double npos) { pos[2] = npos; }

	inline double GetA() const { return pos[3]; }
	inline void SetA(double npos) { pos[3] = npos; }

	inline double GetB() const { return pos[4]; }
	inline void SetB(double npos) { pos[4] = npos; }

	inline double GetC() const { return pos[5]; }
	inline void SetC(double npos) { pos[5] = npos; }

	/*       private:
		   friend class boost::serialization::access;
		   template<class Archive>
		   void serialize(Archive & ar, const unsigned int version)
		   {
		       ar & pos[0];
		       ar & pos[1];
		       ar & pos[2];
		       ar & pos[3];
		       ar & pos[4];
		       ar & pos[5];
		   }*/

private:
	double pos[6];
};



class EmcController : public xlang::Singleton<EmcController>
{
public:
	enum loadState { lsStartProgress, lsStopProgress, lsLoaded, lsFault, lsUnloaded };

	enum ecState {ecEstop = 1, ecEstopReset , ecOff, ecOn };

	enum eCtlMode { emManual = 1, emAuto, emMDI };

	enum eInterpState { eiIdle = 1, eiReading, eiPaused, eiWaiting };

	enum eLogType { elError, elWarning, elInfo };

public:
	int  GetAxisNumber();
	char GetAxisName(int i);
	int  AxisToIndex(char c);
	bool IsAxisUsed(int i);
	void CreateConfigJSON(const char *fname);

public:
	/*
	 * Update() must be called periodicaly to process error fetching
	 *  and other periodical tasks
	 */
	bool Update();

	void MachineOn();
	void MachineOff();
	int SendJogCont(int axis, double speed);
	int SendJogIncr(int axis, double speed, double incr);
	int SendJogStop(int axis);
	/*
	 * Return absolute or relative position of the axis
	 */

	double GetAxisPosition(char axis);
	Coord GetCurrentOrigin();
	/*
	 * SetPositionAbsolute(bool) switch absolute or relative position
	 *  will be returned from GetAxisPosition() call
	 */
	void SetPositionAbsolute(bool abs) {m_positionAbsolute = abs;}
	bool IsPositionAbsolute() {return m_positionAbsolute;}
	double GetVelocity();
	void SetFeedOverridePercent(int val);
	/*
	 * DoHome() execute homing sequence for specifed @axis
	 */
	void DoHome(char axis);
	/*
	 * GoHome() move axes to the Zero position
	 */
	void GoHome();
	/*
	 *  OverrideLimits() ON/OFF overriding of a Machine soft/hard limits
	 */
	void OverrideLimits(bool);

	bool IsLimitsOverrided();
	/*
	 * Get active G-codes
	 */
	void GetActiveCodes(char *out_buf);

	int GetLineNumber();

	void OpenProgram(const char *name);

	const char *GetLastFileName() const;

	void RunProgram(int line);

	void PauseProgram();

	void ResumeProgram();

	void StepProgram();

	void StopProgram();

public:
	ListPrt GetActualProgramContext();
	uint32_t GetActualContextIndex();
	uint32_t GetCurrPosInContext();
	uint32_t GetNumLinesPerContext();
	bool isContextChanged();

private:
	void InitProgramContext();

public:
	EmcController();

public:
	void SetIniFile(const std::string &filename);

	const std::string &GetEmcPath();

	void SetEmcIni(const std::string &ini);

	const std::string &GetEmcIni();

public:
	loadState Init(const char *config);
	bool InitAsync();
	// Unload EMC from the memory
	void Exit();
	void ExitAsync();
	// Check init status
	loadState GetInitState();

	int ShellCmd(const char *cmd, const char *arg);

private:
	void Init_thread();
	void SetInitState(loadState mode);
	bool Init_internal();
	void Exit_internal();

public:
	/*
	 * Return current controller state
	 */
	ecState GetControllerState();
	/*
	 * Return current controller mode
	 */
	eCtlMode GetControllerMode();

	bool SetControllerMode(eCtlMode);
	/*
	 * Get current interpreter state
	 */
	eInterpState GetInterpState();
	/*
	 * Return true, if axis not moved (in position)
	 */
	bool IsInpos();

	double GetMaxVelocity();

	bool ExecMDI(const char *cmd);

	void SetFixtureOffset(const Coord &coord, int index);

	void SetFixtureOffsetToCurrent(int index);

	void ActivateFixtureOffset(int index);

	void ClearFixtureOffset(int index);

	Coord GetCurrentOffset();

	void Spindle(bool state);

	bool IsSpindleOn();

public:
	virtual ~EmcController();

public:
	// Get number of error string in the Log
	int GetErrorCount(eLogType lType);
	// Return next string from the log and detach from the log
	std::string GetNextError(eLogType lType);

private:
	void AddErrorString(eLogType lType, const char *error);
	bool IsMachineFree();

protected:
	std::string m_iniFile;
	loadState m_loadState;
	std::queue<std::string> m_Log[3];
	bool m_positionAbsolute;
	bool m_overrideLimits;

	bool m_threadExit;
	boost::thread *m_pthread;
	boost::mutex m_guard;

private:
	int m_last_context_index;
	std::map<int,int> m_lines_map;
	StringList m_context;
	std::string m_lastProgramFile;
	std::ifstream m_pgm_file;
	bool m_bForceUpdate;
	std::map<char,bool> m_used_map;
};

}

#endif
