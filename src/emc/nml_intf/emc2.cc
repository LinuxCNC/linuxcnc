/*
*	 File created on Fri Jun 24 00:02:18 BST 2005
*/
/*
*	New C++ File starts here.
*	This file should be named emc2.cc
*/

// Include all NML, CMS, and RCS classes and functions
#include "rcs.hh"

// Include command and status message definitions
#include "canon.hh"
#include "global_defs.h"
#include "emc2.hh"
#include "emcglb.h"
#include "emcpos.h"

// Forward Function Prototypes
void PmCartesian_update(CMS * cms, PmCartesian * x);
void initialize_PmCartesian(PmCartesian * x);
void EmcPose_update(CMS * cms, EmcPose * x);
void initialize_EmcPose(EmcPose * x);

int emcFormat(NMLTYPE type, void *buffer, CMS * cms)
{
    switch (type) {
    case EMC_ABORT_TYPE:
	((EmcAbort *) buffer)->update(cms);
	break;
    case EMC_HALT_TYPE:
	((EmcHalt *) buffer)->update(cms);
	break;
    case EMC_INIT_TYPE:
	((EmcInit *) buffer)->update(cms);
	break;
    case EMC_NULL_TYPE:
	((EmcNull *) buffer)->update(cms);
	break;
    case EMC_OPERATOR_DISPLAY_TYPE:
	((EmcOperatorDisplay *) buffer)->update(cms);
	break;
    case EMC_OPERATOR_ERROR_TYPE:
	((EmcOperatorError *) buffer)->update(cms);
	break;
    case EMC_OPERATOR_TEXT_TYPE:
	((EmcOperatorText *) buffer)->update(cms);
	break;
    case EMC_SET_DEBUG_TYPE:
	((EmcSetDebug *) buffer)->update(cms);
	break;
    case EMC_SYSTEM_COMMAND_TYPE:
	((EmcSystemCommand *) buffer)->update(cms);
	break;

    default:
	return (0);
    }
    return 1;
}

const char *emc_symbol_lookup(long type)
{
    switch (type) {
    case EMC_ABORT_TYPE:
	return "EmcAbort";
    case EMC_HALT_TYPE:
	return "EmcHalt";
    case EMC_INIT_TYPE:
	return "EmcInit";
    case EMC_NULL_TYPE:
	return "EmcNull";
    case EMC_OPERATOR_DISPLAY_TYPE:
	return "EmcOperatorDisplay";
    case EMC_OPERATOR_ERROR_TYPE:
	return "EmcOperatorError";
    case EMC_OPERATOR_TEXT_TYPE:
	return "EmcOperatorText";
    case EMC_SET_DEBUG_TYPE:
	return "EmcSetDebug";
    case EMC_SYSTEM_COMMAND_TYPE:
	return "EmcSystemCommand";
    default:
	return "UNKNOWN";
	break;
    }
    return (NULL);
}

void EmcHalt::update(CMS * cms)
{

}

void EmcSystemCommand::update(CMS * cms)
{
    cms->update(string, 256);

}

void EmcOperatorError::update(CMS * cms)
{
    cms->update(id);
    cms->update(error, 256);

}

void EmcNull::update(CMS * cms)
{

}

void EmcSetDebug::update(CMS * cms)
{
    cms->update(debug);

}

void EmcInit::update(CMS * cms)
{

}

void PmCartesian_update(CMS * cms, PmCartesian * x)
{
    cms->update(x->x);
    cms->update(x->y);
    cms->update(x->z);

}

void EmcOperatorDisplay::update(CMS * cms)
{
    cms->update(id);
    cms->update(display, 256);

}

void EmcOperatorText::update(CMS * cms)
{
    cms->update(id);
    cms->update(text, 256);

}

void EmcPose_update(CMS * cms, EmcPose * x)
{
    PmCartesian_update(cms, &(x->tran));
    cms->update(x->a);
    cms->update(x->b);
    cms->update(x->c);

}

void EmcAbort::update(CMS * cms)
{

}
