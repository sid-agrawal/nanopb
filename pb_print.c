/* pb_encode.c -- encode a protobuf using minimal resources
 *
 * 2011 Petteri Aimonen <jpa@kapsi.fi>
 */

#include "pb.h"
#include "pb_print.h"
#include "pb_common.h"
#include "pb_encode.h"
#include <stdio.h>

/* Use the GCC warn_unused_result attribute to check that all return values
 * are propagated correctly. On other compilers and gcc before 3.4.0 just
 * ignore the annotation.
 */
#if !defined(__GNUC__) || (__GNUC__ < 3) || (__GNUC__ == 3 && __GNUC_MINOR__ < 4)
#define checkreturn
#else
#define checkreturn __attribute__((warn_unused_result))
#endif

/**************************************
 * Declarations internal to this file *
 **************************************/
static bool checkreturn print_basic_field(const pb_field_iter_t *field, int indent);
static bool checkreturn print_array(pb_field_iter_t *field, int indent);
static bool checkreturn print_submessage(const pb_field_iter_t *field, int indent);
static bool checkreturn print_field(pb_field_iter_t *field, int indent);
static bool checkreturn pb_print_internal(const pb_msgdesc_t *fields, const void *src_struct, int indent);
static bool field_is_empty(pb_field_iter_t *field);

static bool safe_read_bool(const void *pSize)
{
    const char *p = (const char *)pSize;
    size_t i;
    for (i = 0; i < sizeof(bool); i++)
    {
        if (p[i] != 0)
            return true;
    }
    return false;
}

static bool checkreturn print_int(const pb_field_iter_t *field)
{
    uint64_t value;

    if (field->data_size == sizeof(uint64_t))
        value = *(uint64_t *)field->pData;
    else if (field->data_size == sizeof(uint32_t))
        value = *(uint32_t *)field->pData;
    else if (field->data_size == sizeof(uint_least16_t))
        value = *(uint_least16_t *)field->pData;
    else if (field->data_size == sizeof(uint_least8_t))
        value = *(uint_least8_t *)field->pData;
    else
        return false;

    printf("0x%lx", value);
    return true;
}
/* Print a field with static or pointer allocation, i.e. one whose data
 * is available to the encoder directly. */
static bool checkreturn print_basic_field(const pb_field_iter_t *field, int indent)
{
    const pb_bytes_array_t *bytes_array;
    const pb_byte_t *bytes;
    if (!field->pData)
    {
        /* Missing pointer field */
        return true;
    }

    switch (PB_LTYPE(field->type))
    {
    case PB_LTYPE_BOOL:
        printf("%s", safe_read_bool(field->pData) ? "true" : "false");
        return true;

    case PB_LTYPE_VARINT:
    case PB_LTYPE_UVARINT:
    case PB_LTYPE_SVARINT:
        return print_int(field);

    case PB_LTYPE_FIXED32:
        bytes = (const pb_byte_t *)field->pData;
        for (int i = 0; i < 4; i++)
        {
            printf("%x", bytes[i] & 0xf);
        }
        return true;
    case PB_LTYPE_FIXED64:
        bytes = (const pb_byte_t *)field->pData;
        for (int i = 0; i < 8; i++)
        {
            printf("%x", bytes[i] & 0xf);
        }
        return true;

    case PB_LTYPE_BYTES:
        bytes_array = (const pb_bytes_array_t *)field->pData;
        for (int i = 0; i < bytes_array->size; i++)
        {
            printf("%x", bytes_array->bytes[i] & 0xf);
        }
        return true;

    case PB_LTYPE_STRING:
        //bytes_array = (const pb_bytes_array_t *)field->pData;
        //for (int i = 0; i < bytes_array->size; i++)
         //{
        //    printf("%c", bytes_array->bytes[i] & 0xf);
        //}
        printf("%s", (char *)field->pData);
        return true;

    case PB_LTYPE_SUBMESSAGE:
        printf("\n");
        bool error = print_submessage(field, indent);
        return error;

    case PB_LTYPE_SUBMSG_W_CB:
        return false;

    case PB_LTYPE_FIXED_LENGTH_BYTES:
        bytes = (const pb_byte_t *)field->pData;
        for (int i = 0; i < (size_t)field->data_size; i++)
        {
            printf("%x", bytes[i] & 0xf);
        }
        return true;

    default:
        return false;
    }
}

/* Encode a static array. Handles the size calculations and possible packing. */
static bool checkreturn print_array(pb_field_iter_t *field, int indent)
{
    pb_size_t i;
    pb_size_t count;
    size_t size;

    count = *(pb_size_t *)field->pSize;

    printf("[");
    for (i = 0; i < count; i++)
    {
        /* Normally the data is stored directly in the array entries, but
         * for pointer-type string and bytes fields, the array entries are
         * actually pointers themselves also. So we have to dereference once
         * more to get to the actual data. */
        bool error = print_basic_field(field, indent);
        printf(",");

        if (error == false)
        {
            return error;
        }

        field->pData = (char *)field->pData + field->data_size;
    }
    printf("]");

    return true;
}

static bool field_is_empty(pb_field_iter_t *field)
{
    /* Check field presence */
    if (PB_HTYPE(field->type) == PB_HTYPE_ONEOF)
    {
        if (*(const pb_size_t *)field->pSize != field->tag)
        {
            /* Different type oneof field */
            return true;
        }
    }
    else if (PB_HTYPE(field->type) == PB_HTYPE_OPTIONAL)
    {
        if (field->pSize)
        {
            if (safe_read_bool(field->pSize) == false)
            {
                /* Missing optional field */
                return true;
            }
        }
        else if (PB_ATYPE(field->type) == PB_ATYPE_STATIC)
        {
            /* Proto3 singular field */
            if (pb_check_proto3_default_value(field))
                return true;
        }
    }

    if (!field->pData)
    {
        if (PB_HTYPE(field->type) == PB_HTYPE_REQUIRED)
            return false;

        /* Pointer field set to NULL */
        return true;
    }

    if (!field->pData)
    {
        if (PB_HTYPE(field->type) == PB_HTYPE_REQUIRED)
            return false;

        /* Pointer field set to NULL */
        return true;
    }
}

/* Print a single field of any callback, pointer or static type. */
static bool checkreturn print_field(pb_field_iter_t *field, int indent)
{
    if (PB_ATYPE(field->type) == PB_ATYPE_CALLBACK)
    {
        return false;
    }
    else if (PB_HTYPE(field->type) == PB_HTYPE_REPEATED)
    {
        return print_array(field, indent);
    }
    else
    {
        return print_basic_field(field, indent);
    }
}

static bool checkreturn print_submessage(const pb_field_iter_t *field, int indent)
{
    if (field->submsg_desc == NULL)
        return false;

    return pb_print_internal(field->submsg_desc, field->pData, indent + 1);
}

static bool checkreturn pb_print_internal(const pb_msgdesc_t *fields, const void *src_struct, int indent)
{
    pb_field_iter_t iter;
    int i = 0;

    if (!pb_field_iter_begin_const(&iter, fields, src_struct))
        return true; /* Empty message type */

    do
    {
        if (PB_LTYPE(iter.type) == PB_LTYPE_EXTENSION)
        {
            return false;
        }
        else if (!field_is_empty(&iter))
        {
            /* Regular field */
            for (int j = 0; j < indent; j++)
            {
                printf("    ");
            }
            printf("- %s: ", fields->field_names[i]);
            if (!print_field(&iter, indent))
                return false;
            printf("\n");
        }
        i++;
    } while (pb_field_iter_next(&iter));
    printf("\n");
}

bool checkreturn pb_pretty_print(const pb_msgdesc_t *fields, const void *src_struct)
{
    return pb_print_internal(fields, src_struct, 0);
}