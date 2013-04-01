// some useful helpers for dealing with introspection

// return the number of remote components
// which are currently unbound
extern int num_unbound_components();

// the number of pins this component exports
extern int num_pins(const char *comp_name);
