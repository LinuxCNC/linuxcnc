#ifndef _HAL_OBJECT_SELECTORS_H
#define _HAL_OBJECT_SELECTORS_H


// retrieving a HAL object descriptor, or any field therein, is a two-step
// process:
// 1. define a standard object selection.
//   (a) selecting the object type (optional):
//     .type = <desired type>, // one of enum hal_object_type,
//                             // or 0 for any object type
//
//   (b) selecting the actual object (an 'or' of all conditions):
//     .id = <object ID>,        // by ID, OR
//     .owner_id = <object ID>,  // by owner ID, OR
//     .name = <object name>     // by name
//
// 2. once the object(s) are selected,
//    apply the callback function on each member of the result set,
//    optionally passing selector-specific arguments and
//    retrieving selector-specific return values
//    through the foreach_args struct.
//    there are two user-definable int params (user_arg1 and user_arg2)
//    plus two user-definable void pointers (user_ptr1 and user_ptr2).
//    these can be extended as needed.

//--------------------------------------------------------------------
// use this selector to retrieve the name of a HAL object
// selected by the standard selection (type, object ID/object name prefix):
//
// selector-specific arguments:
//
// returned name or NULL if not found
//     .user_ptr1 = NULL,
// };
int yield_match(hal_object_ptr o, foreach_args_t *args);


//--------------------------------------------------------------------
// use this selector to count the number of objects matching
// the standard selection (type, object ID/object name):
//
// halg_foreach() returns the count.
// };
int yield_count(hal_object_ptr o, foreach_args_t *args);

//--------------------------------------------------------------------
// use this selector to retrieve a pointer to a HAL vtable descriptor
// selected by standard selection AND having a particular version:
//     .type = HAL_VTABLE,  // type MUST be present
//     .name = name,        // if selecting by name AND/OR
//     .id = <object ID>,   // by vtable object ID
//
// required argument:
//     .user_arg1 = <version>,
//
// returned vtable descriptor or NULL if not found
//     .user_ptr1 = NULL,
// };
int yield_versioned_vtable_object(hal_object_ptr o, foreach_args_t *args);


//--------------------------------------------------------------------
// use this selector to support the halpr_foreach_<type> iterators
//
// required argument:
//     .user_ptr1 = <callback>,
//     .user_ptr2 = <cb_data>,
int yield_foreach(hal_object_ptr o, foreach_args_t *args);

//--------------------------------------------------------------------
// use this selector to count the number of objects owned
// by a component
//
// this honors the dual-ownership semantics of pins/params/functs
// as implemented in halpr_find_owning_comp()
//
// the comp id is passed in args->user_arg1
// the count is returned in args->user_arg2
int yield_count_owned_by_comp(hal_object_ptr o, foreach_args_t *args);

//--------------------------------------------------------------------
// use this selector to delete HAL objects from an iterator which holds
// the HAL mutex.
// this only works for objects which support a conditionally locked
// halg_delete_<object> method, currently:
// HAL_SIGNAL
int unlocked_delete_halobject(hal_object_ptr o, foreach_args_t *args);


//----------- HAL library internal use only below --------------------
// use this selector to free any number of matching HAL objects,
// of type HAL_PIN HAL_PARAM HAL_INST HAL_THREAD HAL_FUNCT etc
// (HAL library internal use only)
//
// required argument:
//     .user_ptr1 = <callback>,
//     .user_ptr2 = <cb_data>,
int yield_free(hal_object_ptr o, foreach_args_t *args);



#endif
