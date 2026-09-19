/********************************************************************
 * Description: interpl_nml.hh
 *   Binds the generic InterpList queue to the NML message classes.
 *
 *   This is the only place the interpreter list and NML know about each
 *   other. When the NML message classes are replaced, this header goes
 *   with them and InterpList is instantiated on whatever succeeds them;
 *   interpl.hh itself needs no change.
 *
 * License: GPL Version 2
 * System: Linux
 ********************************************************************/
#ifndef INTERPL_NML_HH
#define INTERPL_NML_HH

#include "interpl.hh"
#include "emc.hh"               // emc_symbol_lookup()
#include "libnml/nml/nmlmsg.hh" // class NMLmsg

template <>
struct InterpListTraits<NMLmsg> {
    static bool valid(const NMLmsg &msg)
    {
        // _type 0 is NML's "unset" type, and nothing smaller than the NML
        // header can be a real message.
        return msg._type != 0 && msg.size >= 4;
    }

    static const char *name(const NMLmsg &msg)
    {
        return emc_symbol_lookup(msg._type);
    }
};

using NML_INTERP_LIST = InterpList<NMLmsg>;

extern NML_INTERP_LIST interp_list; /* NML Union, for interpreter */

#endif
