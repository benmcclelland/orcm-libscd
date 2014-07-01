/*
 * Copyright (c) 2004-2010 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2011 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006-2013 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007      Sun Microsystems, Inc.  All rights reserved.
 * Copyright (c) 2007      Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * Copyright (c) 2010      Oracle and/or its affiliates.  All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <stdio.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif  /*  HAVE_STDLIB_H */
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif  /* HAVE_SYS_STAT_H */
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif  /* HAVE_SYS_WAIT_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif  /* HAVE_DIRENT_H */

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "opal/mca/event/event.h"
#include "opal/util/basename.h"
#include "opal/util/cmd_line.h"
#include "opal/util/output.h"
#include "opal/util/opal_environ.h"
#include "opal/util/show_help.h"
#include "opal/mca/base/base.h"
#include "opal/runtime/opal.h"
#if OPAL_ENABLE_FT_CR == 1
#include "opal/runtime/opal_cr.h"
#endif

#include "orte/util/error_strings.h"
#include "orte/util/show_help.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"

#include "orcm/runtime/runtime.h"

#include "orcm/mca/scd/base/base.h"

typedef struct {
    char *name;
    orcm_node_state_t state;
} liborcm_node_t;

/******************
 * API Functions
 ******************/
int get_nodes(liborcm_node_t ***nodes, int *count);

/******************
 * Local Functions
 ******************/
static int orcm_libscd_init();
static int orcm_libscd_finalize();


static int orcm_libscd_init() 
{
    return orcm_init(ORCM_TOOL);
}

static int orcm_libscd_finalize() 
{
    //return orcm_finalize();
    return 0;
}

int get_nodes(liborcm_node_t ***nodes, int *count)
{
    orte_rml_recv_cb_t xfer;
    opal_buffer_t *buf;
    orcm_node_t **orcmnodes;
    int rc, i, n, num_nodes;
    orcm_scd_cmd_flag_t command;

    if (ORCM_SUCCESS != (rc = orcm_libscd_init())) {
        fprintf(stderr, "Failed init\n");
        return -1;
    }

    /* setup to receive the result */
    OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
    xfer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_SCD,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xfer);

    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_NODE_INFO_COMMAND;
    /* pack the alloc command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command, 1, ORCM_SCD_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        return -1;
    }
    /* send it to the scheduler */
    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buf,
                                                      ORCM_RML_TAG_SCD,
                                                      orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return -1;
    }

    /* unpack number of nodes */
    ORTE_WAIT_FOR_COMPLETION(xfer.active);
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &num_nodes, &n, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&xfer);
        return -1;
    }

    if (0 < num_nodes) {
        orcmnodes = (orcm_node_t**)malloc(num_nodes * sizeof(orcm_node_t*));
        *nodes = (liborcm_node_t**)malloc(num_nodes * sizeof(liborcm_node_t*));
        if (NULL == nodes) {
            return -1;
        }

        /* unpack the nodes */
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, orcmnodes, &num_nodes, ORCM_NODE))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return -1;
        }
        for (i = 0; i < num_nodes; i++) {
            (*nodes)[i] = (liborcm_node_t*)malloc(sizeof(liborcm_node_t*));
            (*nodes)[i]->name = strdup(orcmnodes[i]->name);
            (*nodes)[i]->state = orcmnodes[i]->state;
        }
    }

    OBJ_DESTRUCT(&xfer);

    if (ORCM_SUCCESS != orcm_libscd_finalize()) {
        fprintf(stderr, "Failed finalize\n");
        return -1;
    }

    *count = num_nodes;

    return 0;
}
