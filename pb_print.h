/* pb_print.h: Functions to print protocol buffers. Depends on pb_print.c.
 * The main function is pb_pretty_print. 
 You also need the field descriptions created by nanopb_generator.py.
 */

#ifndef PB_PRINT_H_INCLUDED
#define PB_PRINT_H_INCLUDED

#include "pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************
 * Main printing functions *
 ***************************/

/* Print a single protocol buffers message from C structure to standard output.
 * Returns true on success, false on any failure.
 * The actual struct pointed to by src_struct must match the description in fields.
 * All required fields in the struct are assumed to have been filled in.
 *
 * Example usage:
 *    MyMessage msg = {};
 *    uint8_t buffer[64];
 *    pb_ostream_t stream;
 *
 *    msg.field1 = 42;
 *    stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
 *    pb_encode(&stream, MyMessage_fields, &msg);
 */
bool pb_pretty_print(const pb_msgdesc_t *fields, const void *src_struct);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
