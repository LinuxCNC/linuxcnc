
#ifndef NMLQR_HH
#define NMLQR_HH

#include "nml.hh"
#include "nmlmsg.hh"

class NML_QUERY_MSG:public NMLmsg {
  public:
    NML_QUERY_MSG(NMLTYPE t, long s):NMLmsg(t, s) {
    };
    void update(CMS *);
    int subdiv_for_reply;
};

class NML_QUERY_CHANNEL:public NML {
  public:
    NML_QUERY_CHANNEL(NML_FORMAT_PTR, char *, char *, char *,
	int set_to_server = 0);
     ~NML_QUERY_CHANNEL();
    NML_QUERY_MSG *get_address() {
	return ((NML_QUERY_MSG *) NML::get_address());
    };
};

class NML_ID_CHANNEL:public NML {
  public:
    NML_ID_CHANNEL(NML_FORMAT_PTR, char *, char *, char *,
	int set_to_server = 0);
    ~NML_ID_CHANNEL();
};

#define NML_NO_TIMEOUT (-1.0)

class NML_QR_SERVER {
  public:
    NML_QR_SERVER(NML_FORMAT_PTR f_ptr, char *qr_name, char *process_name,
	char *config_file);
     ~NML_QR_SERVER();

    NMLTYPE readQuery();
    NMLTYPE waitForQuery(double timeout);
    NMLmsg *getQueryAddress();
    int replyToLastQuery(NMLmsg * message_to_send);
    int valid();
    int reset();

  protected:
      NML * replyChannel;
    NML_QUERY_CHANNEL *queryChannel;
    NML_ID_CHANNEL *idChannel;
    int reply_subdiv;
    char *subdiv_allocation_table;
};

class NML_QR_CLIENT {
  public:
    NML_QR_CLIENT(NML_FORMAT_PTR f_ptr, char *qr_name, char *process_name,
	char *config_file);
     ~NML_QR_CLIENT();

    int sendQuery(NML_QUERY_MSG *);
    NMLTYPE readReply();
    NMLTYPE waitForReply(double timeout);
    NMLmsg *getReplyAddress();
    int valid();
    int reset();

  protected:
      NML * replyChannel;
    NML_QUERY_CHANNEL *queryChannel;
    NML_ID_CHANNEL *idChannel;
    int reply_subdiv;
};

//NMLQR_HH
#endif
