#ifndef LINKED_LIST_HH
#define LINKED_LIST_HH

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

#ifdef __cplusplus
}
#endif

enum LIST_SIZING_MODE
{
    DELETE_FROM_HEAD,
    DELETE_FROM_TAIL,
    STOP_AT_MAX,
    NO_MAXIMUM_SIZE
};

class RCS_LINKED_LIST_NODE
{
  public:
    void *data;
    size_t size;
    int id;
    int copied;
    RCS_LINKED_LIST_NODE *next;
    RCS_LINKED_LIST_NODE *last;
    friend class LINKED_LIST;
      RCS_LINKED_LIST_NODE(void *_data, size_t _size);
     ~RCS_LINKED_LIST_NODE();
};

class RCS_LINKED_LIST
{
  protected:
    RCS_LINKED_LIST_NODE * head;
    RCS_LINKED_LIST_NODE *tail;
    RCS_LINKED_LIST_NODE *current_node;
    RCS_LINKED_LIST_NODE *extra_node;
    int next_node_id;
  public:
    int get_current_id();
    int list_size;
    int max_list_size;
    LIST_SIZING_MODE sizing_mode;
    void set_list_sizing_mode(int, LIST_SIZING_MODE);
    void set_max_list_size(int);
    size_t last_size_retrieved;
    int delete_data_not_copied;
    void *last_data_retrieved;
    int last_copied_retrieved;
    void *retrieve_head();
    size_t last_size_stored;
    void *last_data_stored;
    int store_at_head(void *_data, size_t _size, int _copy);
    int store_at_tail(void *_data, size_t _size, int _copy);
    int get_newest_id()
    {
	return (next_node_id - 1);
    }
    void *get_head();
    void *get_tail();
    void *get_next();
    void *find_node(int _node_number);
    void delete_node(int _id);
    void delete_current_node();
    void flush_list();
    void delete_members();
    RCS_LINKED_LIST();
    ~RCS_LINKED_LIST();

  private:
    RCS_LINKED_LIST(RCS_LINKED_LIST & list);	// Don't copy me.
};

#endif /* LINKED_LIST_HH */
