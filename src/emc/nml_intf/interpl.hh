
#ifndef INTERP_LIST_HH
#define INTERP_LIST_HH

/*
  Mechanism for building lists of arbitrary NML messages, used by
  the canonical interface and interpreter to pass planned sequences
  to the HME.

  Modification history:

  25-Jun-2001 made commandbuf part of a union and add dummy variable
  to ensure alignment.
  25-Jul-1997  FMP changed line number stuff;
  changed MAX_EMC_NML_COMMAND_SIZE to MAX_NML_COMMAND_SIZE (I'm trying
  to get the EMC prejudice out of here)
  22-Oct-1996 WPS attempted to fix alignment problems by moving the
  command member of NML_INTERP_LIST_NODE to the begining of the structure.
  16-Nov-1995  Fred Proctor created
  */

#define MAX_NML_COMMAND_SIZE 1000

// these go on the interp list
struct NML_INTERP_LIST_NODE
{
  int line_number;              // line number it was on
  union _dummy_union {
    int i; 
    long l;
    double d;
    float f;
    long long ll;
    long double ld;
  } dummy; // paranoid alignment variable.

  union _command_union {
    char commandbuf[MAX_NML_COMMAND_SIZE]; // the NML command;
    int i; 
    long l;
    double d;
    float f;
    long long ll;
    long double ld;
  } command;
};

// here's the interp list itself
class NML_INTERP_LIST
{
 public:
  NML_INTERP_LIST();
  ~NML_INTERP_LIST();

  int set_line_number(int line);
  int get_line_number();
  int append(NMLmsg &);
  int append(NMLmsg *);
  NMLmsg *get();
  void clear();
  void print();
  int len();

 private:
  LinkedList *linked_list_ptr;
  NML_INTERP_LIST_NODE temp_node; // filled in and put on the list
  int next_line_number;         // line number used to fill temp_node
  int line_number;              // line number of node from get()
};

extern NML_INTERP_LIST interp_list;     /* NML Union, for interpreter */

#endif
