/*
 * Core Definitions for QAPI Visitor implementations
 *
 * Copyright (C) 2012-2016 Red Hat, Inc.
 *
 * Author: Paolo Bonizni <pbonzini@redhat.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */
#ifndef QAPI_VISITOR_IMPL_H
#define QAPI_VISITOR_IMPL_H

#include "qapi/error.h"
#include "qapi/visitor.h"

struct Visitor
{
    /* Must be set */
    void (*start_struct)(Visitor *v, void **obj, const char *kind,
                         const char *name, size_t size, Error **errp);
    void (*end_struct)(Visitor *v, Error **errp);

    void (*start_implicit_struct)(Visitor *v, void **obj, size_t size,
                                  Error **errp);
    void (*end_implicit_struct)(Visitor *v, Error **errp);

    void (*start_list)(Visitor *v, const char *name, Error **errp);
    GenericList *(*next_list)(Visitor *v, GenericList **list, Error **errp);
    void (*end_list)(Visitor *v, Error **errp);

    void (*type_enum)(Visitor *v, int *obj, const char * const strings[],
                      const char *kind, const char *name, Error **errp);
    /* May be NULL; only needed for input visitors. */
    void (*get_next_type)(Visitor *v, QType *type, bool promote_int,
                          const char *name, Error **errp);

    /* Must be set. */
    void (*type_int64)(Visitor *v, int64_t *obj, const char *name,
                       Error **errp);
    /* Must be set. */
    void (*type_uint64)(Visitor *v, uint64_t *obj, const char *name,
                        Error **errp);
    /* Optional; fallback is type_uint64().  */
    void (*type_size)(Visitor *v, uint64_t *obj, const char *name,
                      Error **errp);
    /* Must be set. */
    void (*type_bool)(Visitor *v, bool *obj, const char *name, Error **errp);
    void (*type_str)(Visitor *v, char **obj, const char *name, Error **errp);
    void (*type_number)(Visitor *v, double *obj, const char *name,
                        Error **errp);
    void (*type_any)(Visitor *v, QObject **obj, const char *name,
                     Error **errp);

    /* May be NULL; most useful for input visitors. */
    void (*optional)(Visitor *v, bool *present, const char *name);

    bool (*start_union)(Visitor *v, bool data_present, Error **errp);
};

void input_type_enum(Visitor *v, int *obj, const char * const strings[],
                     const char *kind, const char *name, Error **errp);
void output_type_enum(Visitor *v, int *obj, const char * const strings[],
                      const char *kind, const char *name, Error **errp);

#endif
