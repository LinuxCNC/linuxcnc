
/*
  interplist.cc

  Mechanism for queueing NML messages, used by the interpreter and
  canonical interface to report list of NML statements from program
  files to HME.

  Modification history:

  11-Dec-2000 WPS put in some checks in NML_INTERP_LIST append to catch invalid
changes to the node structure and command messages that are too big. Also modified things so that we conserve memory by not always storing 1000 bytes for every message.
  25-Jul-1997  FMP removed nml_emc.hh dependency, changing how line
  numbers are done. Now they need to be given via set_line_number instead
  of read by interp_wm_line(). Got rid of non-reentrant get_last()
  14-Nov-1996 WPS corrected the size passed to store_at_tail which caused
  a "Read Invalid" error from TestCenter
  16-Nov-1995  Fred Proctor created
  */

extern "C" {
#include <string.h>             /* memcpy() */
}

#include "rcs.hh"               // RCS_LINKED_LIST
#include "interpl.hh"           // these decls
#include "emcglb.h"

NML_INTERP_LIST interp_list;    /* NML Union, for interpreter */

 NML_INTERP_LIST::NML_INTERP_LIST()
{
  linked_list_ptr = new LinkedList;

  next_line_number = 0;
  line_number = 0;
}

 NML_INTERP_LIST::~NML_INTERP_LIST()
{
  if (NULL != linked_list_ptr)
  {
    delete linked_list_ptr;
    linked_list_ptr = NULL;
  }
}

int NML_INTERP_LIST::append(NMLmsg &nml_msg)
{
  /* check for invalid data */
  if (0 == nml_msg.type)
  {
    rcs_print_error("NML_INTERP_LIST::append : attempt to append 0 type\n");
    return -1;
  }

  if (NULL == linked_list_ptr)
  {
    return -1;
  }

  if(nml_msg.size > MAX_NML_COMMAND_SIZE -64)
    {
      rcs_print_error("NML_INTERP_LIST::append : command size is too large.");
      return -1;
    }
  if(nml_msg.size < 4 ) 
    {
      rcs_print_error("NML_INTERP_LIST::append : command size is invalid.");
      return -1;
    }
#ifdef DEBUG_INTERPL
  if(sizeof(temp_node) < MAX_NML_COMMAND_SIZE+4 ||
     sizeof(temp_node) > MAX_NML_COMMAND_SIZE+16   )
    {
      rcs_print_error("NML_INTERP_LIST::append : assumptions about NML_INTERP_LIST_NODE have been violated.\n");
      return -1;
    }
#endif

  // fill in the NML_INTERP_LIST_NODE
  temp_node.line_number = next_line_number;
  memcpy(temp_node.command.commandbuf, &nml_msg, nml_msg.size);

  // stick it on the list
  linked_list_ptr->store_at_tail(&temp_node, nml_msg.size+sizeof(temp_node.line_number)+sizeof(temp_node.dummy)+32+(32-nml_msg.size%32), 1);

#ifdef DEBUG_INTERPL
  if(EMC_DEBUG & EMC_DEBUG_INTERP_LIST)
    {
      rcs_print("NML_INTERP_LIST::append(nml_msg{size=%d,type=%d}) : list_size=%d, line_number = %d\n",
		nml_msg.size,nml_msg.type,linked_list_ptr->list_size,
		temp_node.line_number);
    }
#endif

  return 0;
}

// sets the line number used for subsequent appends
int NML_INTERP_LIST::set_line_number(int line)
{
  next_line_number = line;

  return 0;
}

int NML_INTERP_LIST::append(NMLmsg *nml_msg_ptr)
{
  /* check for invalid data */
  if (NULL == nml_msg_ptr)
  {
    rcs_print_error("NML_INTERP_LIST::append : attempt to append NULL msg\n");
    return -1;
  }

  if (0 == nml_msg_ptr->type)
  {
    rcs_print_error("NML_INTERP_LIST::append : attempt to append 0 type\n");
    return -1;
  }

  if(nml_msg_ptr->size > MAX_NML_COMMAND_SIZE -64 )
    {
      rcs_print_error("NML_INTERP_LIST::append : command size is too large.");
      return -1;
    }
  if(nml_msg_ptr->size < 4 ) 
    {
      rcs_print_error("NML_INTERP_LIST::append : command size is invalid.");
      return -1;
    }
#ifdef DEBUG_INTERPL
  if(sizeof(temp_node) < MAX_NML_COMMAND_SIZE+4 ||
     sizeof(temp_node) > MAX_NML_COMMAND_SIZE+16 ||
     ((void *) &temp_node.line_number) > ((void *) &temp_node.command.commandbuf))
    {
      rcs_print_error("NML_INTERP_LIST::append : assumptions about NML_INTERP_LIST_NODE have been violated.");
      return -1;
    }
#endif

  if (NULL == linked_list_ptr)
  {
    return -1;
  }

  // fill in the NML_INTERP_LIST_NODE
  temp_node.line_number = next_line_number;
  memcpy(temp_node.command.commandbuf, nml_msg_ptr, nml_msg_ptr->size);

  // stick it on the list
  linked_list_ptr->store_at_tail(&temp_node, nml_msg_ptr->size+sizeof(temp_node.line_number)+sizeof(temp_node.dummy)+32+(32-nml_msg_ptr->size%32), 1);

#ifdef DEBUG_INTERPL
  if(EMC_DEBUG & EMC_DEBUG_INTERP_LIST)
    {
      rcs_print("NML_INTERP_LIST::append(nml_msg{size=%d,type=%d}) : list_size=%d, line_number = %d\n",
		nml_msg_ptr->size,nml_msg_ptr->type,linked_list_ptr->list_size,
		temp_node.line_number);
    }
#endif

  return 0;
}

NMLmsg * NML_INTERP_LIST::get()
{
  NMLmsg *ret;
  NML_INTERP_LIST_NODE *node_ptr;

  if (NULL == linked_list_ptr)
  {
    line_number = 0;
    return NULL;
  }

  node_ptr = (NML_INTERP_LIST_NODE *) linked_list_ptr->retrieve_head();

  if (NULL == node_ptr)
  {
    line_number = 0;
    return NULL;
  }

  // save line number of this one, for use by get_line_number
  line_number = node_ptr->line_number;

  // get it off the front
  ret = (NMLmsg *) ((char *) node_ptr->command.commandbuf);

  return ret;
}

void NML_INTERP_LIST::clear()
{
  if (NULL != linked_list_ptr)
  {
    linked_list_ptr->delete_members();
  }
}

void NML_INTERP_LIST::print()
{
  NMLmsg *nml_msg;

  if (NULL == linked_list_ptr)
  {
    return;
  }

  nml_msg = (NMLmsg *) linked_list_ptr->get_head();

  while (NULL != nml_msg)
  {
    rcs_print("%d ", nml_msg->type);
    nml_msg = (NMLmsg *)linked_list_ptr->get_next();
  }

  rcs_print("\n");
}

int NML_INTERP_LIST::len()
{
  if (NULL == linked_list_ptr)
  {
    return 0;
  }

  return ((int) linked_list_ptr->list_size);
}

int NML_INTERP_LIST::get_line_number()
{
  return line_number;
}
