#ifndef TASKCLASS_HH
#define TASKCLASS_HH

#include "emc.hh"
#include "initool.hh"
#include "tool_parse.h"
#include "inifile.hh"

class Task {
public:
    Task();
    virtual ~Task();

    virtual int emcIoInit();
    virtual int emcIoHalt();
    virtual int emcIoAbort(int reason);
    virtual int emcToolStartChange();
    virtual int emcAuxEstopOn();
    virtual int emcAuxEstopOff();
    virtual int emcCoolantMistOn();
    virtual int emcCoolantMistOff();
    virtual int emcCoolantFloodOn();
    virtual int emcCoolantFloodOff();
    virtual int emcLubeOn();
    virtual int emcLubeOff();
    virtual int emcIoSetDebug(int debug);
    virtual int emcToolSetOffset(int pocket, int toolno, EmcPose offset, double diameter,
				 double frontangle, double backangle, int orientation);
    virtual int emcIoUpdate(EMC_IO_STAT * stat);


    virtual int emcToolPrepare(int p, int tool);
    virtual int emcToolLoad();
    virtual int emcToolLoadToolTable(const char *file);
    virtual int emcToolUnload();
    virtual int emcToolSetNumber(int number);

    virtual int emcIoPluginCall(int len, const char *msg);


    // carried over from ioControl.cc
    virtual void load_tool(int pocket);
    virtual int saveToolTable(const char *filename,
			      CANON_TOOL_TABLE toolTable[]);

    // virtual int loadTool(IniFile *toolInifile);
    // virtual int saveToolTable(const char *filename,
    // 			      CANON_TOOL_TABLE toolTable[]);
    // virtual void reload_tool_number(int toolno);
    // virtual int DummyiniTool(const char *filename);

    int use_iocontrol;
    int random_toolchanger;
    // int use_legacy_tooltable;
    const char *ini_filename;
    const char *tooltable_filename;
private:

    char *ttcomments[CANON_POCKETS_MAX];
    int fms[CANON_POCKETS_MAX];
};

extern Task *task_methods;

#endif
