
#include "cms.hh"		// class CMS
#include "timer.hh"		// esleep()
#include "nmlqr.hh"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>		/* malloc(), free() */
#include <stddef.h>		/* size_t */
#include <string.h>		/* strcpy(), strlen(),memcpy() */
#include <stdio.h>		// sprintf

#ifdef __cplusplus
}
#endif
#define ID_REQUEST_TYPE 9991
#define ID_REPLY_TYPE 9992
#define ID_DELETE_TYPE 9993
class ID_REQUEST:public NML_QUERY_MSG {
  public:
    ID_REQUEST():NML_QUERY_MSG(ID_REQUEST_TYPE, sizeof(ID_REQUEST)) {
    };
    void update(CMS *);
};

void
  ID_REQUEST::update(CMS * cms)
{
}

class ID_REPLY:public NML_QUERY_MSG {
  public:
    ID_REPLY():NML_QUERY_MSG(ID_REPLY_TYPE, sizeof(ID_REPLY)) {
    };
    void update(CMS *);
    int subdiv;
};

void ID_REPLY::update(CMS * cms)
{
    cms->update(subdiv);
}

class ID_DELETE:public NML_QUERY_MSG {
  public:
    ID_DELETE():NML_QUERY_MSG(ID_DELETE_TYPE, sizeof(ID_DELETE)) {
    };
    void update(CMS *);
    int subdiv;
};

void ID_DELETE::update(CMS * cms)
{
    cms->update(subdiv);
}

void NML_QUERY_MSG::update(CMS * cms)
{
    cms->update(subdiv_for_reply);
}

int queryFormat(NMLTYPE type, void *buf, CMS * cms)
{
    ((NML_QUERY_MSG *) buf)->update(cms);
    return 0;
}

int idFormat(NMLTYPE type, void *buf, CMS * cms)
{
    queryFormat(type, buf, cms);
    switch (type) {
    case ID_REQUEST_TYPE:
	((ID_REQUEST *) buf)->update(cms);
	break;

    case ID_REPLY_TYPE:
	((ID_REPLY *) buf)->update(cms);
	break;

    case ID_DELETE_TYPE:
	((ID_DELETE *) buf)->update(cms);
	break;

    default:
	return 0;
    }
    return 1;
}

NML_QR_SERVER::NML_QR_SERVER(NML_FORMAT_PTR f_ptr,
    char *qr_name, char *process_name, char *config_file)
{

    queryChannel = NULL;
    replyChannel = NULL;
    idChannel = NULL;
    reply_subdiv = -1;
    subdiv_allocation_table = NULL;

    char repbufname[40];
    sprintf(repbufname, "%sReply", qr_name);
    char querybufname[40];
    sprintf(querybufname, "%sQuery", qr_name);
    char idbufname[40];
    sprintf(idbufname, "%sID", qr_name);
    idChannel =
	new NML_ID_CHANNEL(f_ptr, idbufname, process_name, config_file);
    queryChannel =
	new NML_QUERY_CHANNEL(f_ptr, querybufname, process_name, config_file);
    replyChannel = new NML(f_ptr, repbufname, process_name, config_file);

    if (replyChannel->get_total_subdivisions() > 1) {
	subdiv_allocation_table =
	    (char *) malloc(replyChannel->get_total_subdivisions());
    }
    memset(subdiv_allocation_table, 0,
	replyChannel->get_total_subdivisions());
}

NML_QR_SERVER::~NML_QR_SERVER()
{
    if (NULL != queryChannel) {
	delete queryChannel;
	queryChannel = NULL;
    }
    if (NULL != idChannel) {
	delete idChannel;
	idChannel = NULL;
    }
    if (NULL != replyChannel) {
	delete replyChannel;
	replyChannel = NULL;
    }
    if (NULL != subdiv_allocation_table) {
	free(subdiv_allocation_table);
	subdiv_allocation_table = NULL;
    }
}

NMLTYPE NML_QR_SERVER::readQuery()
{
    while (1) {
	NMLTYPE query_read_type;
	switch (query_read_type = queryChannel->read()) {
	case ID_REQUEST_TYPE:
	    {
		int subdiv_found = 0;
		for (int i = 0; i < replyChannel->get_total_subdivisions();
		    i++) {
		    if (subdiv_allocation_table[i] == 0) {
			subdiv_allocation_table[i] = 1;
			ID_REPLY idMsg;
			idMsg.subdiv = i;
			idChannel->write(idMsg);
			subdiv_found = 1;
			break;
		    }
		}
		if (!subdiv_found) {
		    // Send the message we are full.
		    ID_REPLY idMsg;
		    idMsg.subdiv = -1;
		    idChannel->write(idMsg);
		}
	    }
	    break;

	case ID_REPLY_TYPE:
	    return -1;		// Very weird a reply message in the query
	    // buffer.
	    break;

	case ID_DELETE_TYPE:
	    {
		ID_DELETE *idDeleteMsg =
		    (ID_DELETE *) queryChannel->get_address();
		if (idDeleteMsg->subdiv >= 0
		    && idDeleteMsg->subdiv <
		    replyChannel->get_total_subdivisions()) {
		    subdiv_allocation_table[idDeleteMsg->subdiv] = 0;
		}
	    }
	    break;

	default:
	    NML_QUERY_MSG * qMsg =
		(NML_QUERY_MSG *) queryChannel->get_address();
	    reply_subdiv = qMsg->subdiv_for_reply;
	    return query_read_type;
	}
    }
}

NMLTYPE NML_QR_SERVER::waitForQuery(double timeout)
{
    while (1) {
	NMLTYPE query_read_type;
	switch (query_read_type = queryChannel->blocking_read(timeout)) {
	case ID_REQUEST_TYPE:
	    {
		int subdiv_found = 0;
		for (int i = 0; i < replyChannel->get_total_subdivisions();
		    i++) {
		    if (subdiv_allocation_table[i] == 0) {
			subdiv_allocation_table[i] = 1;
			ID_REPLY idMsg;
			idMsg.subdiv = i;
			idChannel->write(idMsg);
			subdiv_found = 1;
			break;
		    }
		}
		if (!subdiv_found) {
		    // Send the message we are full.
		    ID_REPLY idMsg;
		    idMsg.subdiv = -1;
		    idChannel->write(idMsg);
		}
	    }
	    break;

	case ID_REPLY_TYPE:
	    return -1;		// Very weird a reply message in the query
	    // buffer.
	    break;

	case ID_DELETE_TYPE:
	    {
		ID_DELETE *idDeleteMsg =
		    (ID_DELETE *) queryChannel->get_address();
		if (idDeleteMsg->subdiv >= 0
		    && idDeleteMsg->subdiv <
		    replyChannel->get_total_subdivisions()) {
		    subdiv_allocation_table[idDeleteMsg->subdiv] = 0;
		}
	    }
	    break;

	default:
	    NML_QUERY_MSG * qMsg =
		(NML_QUERY_MSG *) queryChannel->get_address();
	    reply_subdiv = qMsg->subdiv_for_reply;
	    return query_read_type;
	}
    }
}

NMLmsg *NML_QR_SERVER::getQueryAddress()
{
    if (NULL == queryChannel) {
	return NULL;
    }
    return queryChannel->get_address();
}

int NML_QR_SERVER::replyToLastQuery(NMLmsg * message_to_send)
{
    if (NULL == replyChannel) {
	return -1;
    }
    if (reply_subdiv < 0
	|| reply_subdiv > replyChannel->get_total_subdivisions()) {
	return -1;
    }
    return replyChannel->write_subdivision(reply_subdiv, message_to_send);
}

int NML_QR_SERVER::valid()
{
    if (!idChannel->valid()) {
	return 0;
    }
    if (!queryChannel->valid()) {
	return 0;
    }
    if (!replyChannel->valid()) {
	return 0;
    }
    return 1;
}

int NML_QR_SERVER::reset()
{
    if (!idChannel->valid()) {
	idChannel->reset();
    }
    if (!queryChannel->valid()) {
	queryChannel->reset();
    }
    if (!replyChannel->valid()) {
	replyChannel->reset();
    }
    return valid();
}

NML_QR_CLIENT::NML_QR_CLIENT(NML_FORMAT_PTR f_ptr, char *qr_name,
    char *process_name, char *config_file)
{
    queryChannel = NULL;
    replyChannel = NULL;
    idChannel = NULL;
    reply_subdiv = -1;

    char repbufname[40];
    sprintf(repbufname, "%sReply", qr_name);
    char querybufname[40];
    sprintf(querybufname, "%sQuery", qr_name);
    char idbufname[40];
    sprintf(idbufname, "%sID", qr_name);
    idChannel =
	new NML_ID_CHANNEL(f_ptr, idbufname, process_name, config_file);
    queryChannel =
	new NML_QUERY_CHANNEL(f_ptr, querybufname, process_name, config_file);
    replyChannel = new NML(f_ptr, repbufname, process_name, config_file);

    if (idChannel->valid() && queryChannel->valid()) {
	ID_REQUEST reqMsg;
	queryChannel->write(reqMsg);
	esleep(0.05);
	while (!idChannel->read()) {
	    esleep(0.05);
	}
	ID_REPLY *idReply = (ID_REPLY *) idChannel->get_address();
	if (idReply->type == ID_REPLY_TYPE) {
	    reply_subdiv = idReply->subdiv;
	}
    }
}

NML_QR_CLIENT::~NML_QR_CLIENT()
{
    if (NULL != queryChannel) {
	if (reply_subdiv >= 0) {
	    ID_DELETE idDeleteMsg;
	    idDeleteMsg.subdiv_for_reply = reply_subdiv;
	    idDeleteMsg.subdiv = reply_subdiv;
	    queryChannel->write(&idDeleteMsg);
	    esleep(0.05);
	}
	delete queryChannel;
	queryChannel = NULL;
    }
    if (NULL != idChannel) {
	delete idChannel;
	idChannel = NULL;
    }
    if (NULL != replyChannel) {
	delete replyChannel;
	replyChannel = NULL;
    }
}

int
  NML_QR_CLIENT::sendQuery(NML_QUERY_MSG * qMsg)
{
    if (NULL == queryChannel) {
	return -1;
    }
    qMsg->subdiv_for_reply = reply_subdiv;
    return queryChannel->write(qMsg);
}

NMLTYPE NML_QR_CLIENT::readReply()
{
    if (NULL == replyChannel || reply_subdiv < 0) {
	return -1;
    }
    return replyChannel->read_subdivision(reply_subdiv);
}

NMLTYPE NML_QR_CLIENT::waitForReply(double timeout)
{
    if (NULL == replyChannel || reply_subdiv < 0) {
	return -1;
    }
    return replyChannel->blocking_read_subdivision(reply_subdiv, timeout);
}

NMLmsg *NML_QR_CLIENT::getReplyAddress()
{
    if (NULL == replyChannel || reply_subdiv < 0) {
	return NULL;
    }
    return replyChannel->get_address_subdivision(reply_subdiv);
}

int NML_QR_CLIENT::valid()
{
    if (!idChannel->valid()) {
	return 0;
    }
    if (!queryChannel->valid()) {
	return 0;
    }
    if (!replyChannel->valid()) {
	return 0;
    }
    if (reply_subdiv < 0) {
	return 0;
    }
    return 1;
}

int NML_QR_CLIENT::reset()
{
    if (!idChannel->valid()) {
	idChannel->reset();
    }
    if (!queryChannel->valid()) {
	queryChannel->reset();
    }
    if (!replyChannel->valid()) {
	replyChannel->reset();
    }
    if (idChannel->valid() && queryChannel->valid() && reply_subdiv < 0) {
	ID_REQUEST reqMsg;
	queryChannel->write(reqMsg);
	esleep(0.05);
	while (!idChannel->read()) {
	    esleep(0.05);
	}
	ID_REPLY *idReply = (ID_REPLY *) idChannel->get_address();
	if (idReply->type == ID_REPLY_TYPE) {
	    reply_subdiv = idReply->subdiv;
	}
    }
    return valid();
}

NML_QUERY_CHANNEL::NML_QUERY_CHANNEL(NML_FORMAT_PTR f_ptr, char *name, char *process, char *file, int set_to_server):
NML(name, process, file, set_to_server)
{
    format_chain = new LinkedList;
    prefix_format_chain(f_ptr);
    prefix_format_chain(idFormat);
    channel_type = NML_QUERY_CHANNEL_TYPE;
    sizeof_message_header = sizeof(NML_QUERY_MSG);

    if (NULL != cms) {
	cms->sizeof_message_header = sizeof_message_header;
    }

    register_with_server();

}

NML_QUERY_CHANNEL::~NML_QUERY_CHANNEL()
{
    // Something funny happens to gdb without this being explicitly defined.
}

NML_ID_CHANNEL::NML_ID_CHANNEL(NML_FORMAT_PTR f_ptr, char *name,
    char *process, char *file, int set_to_server):NML(name, process, file,
    set_to_server)
{
    format_chain = new LinkedList;
    prefix_format_chain(idFormat);
    channel_type = NML_ID_CHANNEL_TYPE;
    sizeof_message_header = sizeof(NML_QUERY_MSG);

    if (NULL != cms) {
	cms->sizeof_message_header = sizeof_message_header;
    }

    register_with_server();

}

NML_ID_CHANNEL::~NML_ID_CHANNEL()
{
    // Something funny happens to gdb without this being explicitly defined.
}
