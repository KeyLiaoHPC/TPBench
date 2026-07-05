/*
 * rafdb-l2-kernel-meta-build.c
 * Build kernel_attr_t headers and default-value data from a registered kernel.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/tpb-public.h"
#include "../tpb-types.h"
#include "rafdb-l1-types.h"
#include "tpb-raf-kernel-meta.h"

/* Local Function Prototypes */
static int _sf_append_parm_value(uint8_t **ptr, const tpb_rt_parm_t *parm,
                                 uint32_t elem_size);
static void _sf_fill_metric_header(tpb_meta_header_t *h,
                                   const tpb_k_output_t *out);
static void _sf_fill_parm_header(tpb_meta_header_t *h,
                                 const tpb_rt_parm_t *parm);
static void _sf_fill_string_header(tpb_meta_header_t *h, const char *name,
                                   const char *payload);
static int _sf_init_meta_payload(char **payload_buf, size_t *payload_cap,
                                 const char *section, const char *initial_kv);

static const char *g_meta_hdr_names[TPB_RAF_KERNEL_META_HDR_COUNT] = {
    TPB_RAF_KERNEL_HDR_VARIATION,
    TPB_RAF_KERNEL_HDR_COMPILATION,
    TPB_RAF_KERNEL_HDR_DEPENDENCY
};

static void
_sf_fill_parm_header(tpb_meta_header_t *h, const tpb_rt_parm_t *parm)
{
    uint32_t type_code;
    uint32_t elem_size;

    memset(h, 0, sizeof(*h));
    h->block_size = TPB_RAF_HDR_FIXED_SIZE;
    h->ndim = 1;
    h->dimsizes[0] = 1;
    type_code = (uint32_t)(parm->ctrlbits & TPB_PARM_TYPE_MASK);
    elem_size = (type_code >> 8) & 0xFF;
    if (elem_size == 0) {
        elem_size = 8;
    }
    h->data_size = elem_size;
    h->type_bits = (uint32_t)(parm->ctrlbits &
        (TPB_PARM_SOURCE_MASK | TPB_PARM_CHECK_MASK | TPB_PARM_TYPE_MASK));
    snprintf(h->name, sizeof(h->name), "%s", parm->name);
    snprintf(h->note, sizeof(h->note), "%s", parm->note);
}

static void
_sf_fill_metric_header(tpb_meta_header_t *h, const tpb_k_output_t *out)
{
    memset(h, 0, sizeof(*h));
    h->block_size = TPB_RAF_HDR_FIXED_SIZE;
    h->ndim = 1;
    h->dimsizes[0] = 1;
    h->data_size = 0;
    if ((uint32_t)(out->dtype & TPB_PARM_TYPE_MASK) ==
        (TPB_DTYPE_TIMER_T & TPB_PARM_TYPE_MASK)) {
        h->type_bits = (uint32_t)(TPB_INT64_T & TPB_PARM_TYPE_MASK);
    } else {
        h->type_bits = (uint32_t)(out->dtype & TPB_PARM_TYPE_MASK);
    }
    h->uattr_bits = (uint64_t)out->unit;
    snprintf(h->name, sizeof(h->name), "%s", out->name);
    snprintf(h->note, sizeof(h->note), "%s", out->note);
}

static int
_sf_append_parm_value(uint8_t **ptr, const tpb_rt_parm_t *parm,
                      uint32_t elem_size)
{
    if (*ptr == NULL || parm == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    memcpy(*ptr, &parm->value, elem_size);
    *ptr += elem_size;
    return TPBE_SUCCESS;
}

static int
_sf_init_meta_payload(char **payload_buf, size_t *payload_cap,
                      const char *section, const char *initial_kv)
{
    size_t need;

    if (payload_buf == NULL || payload_cap == NULL || section == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    need = 64;
    if (initial_kv != NULL) {
        need += strlen(initial_kv);
    }
    *payload_buf = (char *)calloc(1, need);
    if (*payload_buf == NULL) {
        return TPBE_MALLOC_FAIL;
    }
    *payload_cap = need;
    snprintf(*payload_buf, need, "format=tpbench.kernel_meta.v1\nsection=%s\n",
             section);
    if (initial_kv != NULL && initial_kv[0] != '\0') {
        strncat(*payload_buf, initial_kv, need - strlen(*payload_buf) - 1);
    }
    return TPBE_SUCCESS;
}

static void
_sf_fill_string_header(tpb_meta_header_t *h, const char *name,
                       const char *payload)
{
    size_t plen;

    memset(h, 0, sizeof(*h));
    h->block_size = TPB_RAF_HDR_FIXED_SIZE;
    h->ndim = 1;
    plen = (payload != NULL) ? strlen(payload) : 0;
    h->dimsizes[0] = (uint64_t)(plen + 1);
    h->data_size = (uint64_t)(plen + 1);
    h->type_bits = (uint32_t)(TPB_STRING_T & TPB_PARM_TYPE_MASK);
    snprintf(h->name, sizeof(h->name), "%s", name);
    snprintf(h->note, sizeof(h->note), "Kernel metadata: %s", name);
}

/**
 * @brief Build kernel_attr_t headers and default-value data from a registered
 *        kernel.
 */
int
tpb_raf_kernel_build_registered_attr(const tpb_kernel_t *kernel,
                                     const unsigned char kernel_id[20],
                                     kernel_attr_t *attr_out,
                                     void **data_out,
                                     uint64_t *datasize_out)
{
    uint32_t nparm;
    uint32_t nmetric;
    uint32_t nheader;
    uint32_t hi;
    tpb_meta_header_t *hdrs;
    void *data;
    uint64_t datasize;
    uint8_t *ptr;
    char kid_hex[41];
    char var_init[512];
    char *meta_payloads[TPB_RAF_KERNEL_META_HDR_COUNT];
    size_t meta_caps[TPB_RAF_KERNEL_META_HDR_COUNT];
    uint32_t mi;

    if (kernel == NULL || kernel_id == NULL || attr_out == NULL) {
        return TPBE_NULLPTR_ARG;
    }

    nparm = (uint32_t)kernel->info.nparms;
    nmetric = (uint32_t)kernel->info.nouts;
    nheader = nparm + nmetric + TPB_RAF_KERNEL_META_HDR_COUNT;

    hdrs = (tpb_meta_header_t *)calloc(nheader, sizeof(*hdrs));
    if (hdrs == NULL) {
        return TPBE_MALLOC_FAIL;
    }

    datasize = 0;
    for (uint32_t i = 0; i < nparm; i++) {
        uint32_t type_code;
        uint32_t elem_size;

        _sf_fill_parm_header(&hdrs[i], &kernel->info.parms[i]);
        type_code = (uint32_t)(kernel->info.parms[i].ctrlbits &
                               TPB_PARM_TYPE_MASK);
        elem_size = (type_code >> 8) & 0xFF;
        if (elem_size == 0) {
            elem_size = 8;
        }
        datasize += elem_size;
    }

    for (uint32_t j = 0; j < nmetric; j++) {
        _sf_fill_metric_header(&hdrs[nparm + j], &kernel->info.outs[j]);
    }

    tpb_raf_id_to_hex(kernel_id, kid_hex);
    snprintf(var_init, sizeof(var_init),
             "kernel.name=%s\nkernel.id=%s\nactive=1\n",
             kernel->info.name, kid_hex);

    memset(meta_payloads, 0, sizeof(meta_payloads));
    memset(meta_caps, 0, sizeof(meta_caps));
    for (mi = 0; mi < TPB_RAF_KERNEL_META_HDR_COUNT; mi++) {
        const char *init = (mi == 0) ? var_init : NULL;

        if (_sf_init_meta_payload(&meta_payloads[mi], &meta_caps[mi],
                                  g_meta_hdr_names[mi], init) != TPBE_SUCCESS) {
            for (uint32_t k = 0; k < mi; k++) {
                free(meta_payloads[k]);
            }
            free(hdrs);
            return TPBE_MALLOC_FAIL;
        }
        _sf_fill_string_header(&hdrs[nparm + nmetric + mi],
                               g_meta_hdr_names[mi], meta_payloads[mi]);
        datasize += hdrs[nparm + nmetric + mi].data_size;
    }

    data = (datasize > 0) ? calloc(1, (size_t)datasize) : NULL;
    if (datasize > 0 && data == NULL) {
        for (mi = 0; mi < TPB_RAF_KERNEL_META_HDR_COUNT; mi++) {
            free(meta_payloads[mi]);
        }
        free(hdrs);
        return TPBE_MALLOC_FAIL;
    }

    ptr = (uint8_t *)data;
    for (uint32_t i = 0; i < nparm; i++) {
        uint32_t type_code;
        uint32_t elem_size;

        type_code = (uint32_t)(kernel->info.parms[i].ctrlbits &
                               TPB_PARM_TYPE_MASK);
        elem_size = (type_code >> 8) & 0xFF;
        if (elem_size == 0) {
            elem_size = 8;
        }
        _sf_append_parm_value(&ptr, &kernel->info.parms[i], elem_size);
    }

    hi = nparm + nmetric;
    for (mi = 0; mi < TPB_RAF_KERNEL_META_HDR_COUNT; mi++) {
        size_t plen = strlen(meta_payloads[mi]) + 1;
        memcpy(ptr, meta_payloads[mi], plen);
        ptr += plen;
        free(meta_payloads[mi]);
    }

    memset(attr_out, 0, sizeof(*attr_out));
    memcpy(attr_out->kernel_id, kernel_id, 20);
    snprintf(attr_out->kernel_name, sizeof(attr_out->kernel_name), "%s",
             kernel->info.name);
    snprintf(attr_out->version, sizeof(attr_out->version), "%.1f", TPB_VERSION);
    snprintf(attr_out->description, sizeof(attr_out->description), "%s",
             kernel->info.note);
    attr_out->nparm = nparm;
    attr_out->nmetric = nmetric;
    attr_out->kctrl = (uint32_t)kernel->info.kctrl;
    attr_out->nheader = nheader;
    attr_out->active = 1;
    attr_out->headers = hdrs;

    if (data_out != NULL) {
        *data_out = data;
    } else {
        free(data);
    }
    if (datasize_out != NULL) {
        *datasize_out = datasize;
    }
    return TPBE_SUCCESS;
}

/**
 * @brief Free headers allocated by tpb_raf_kernel_build_registered_attr.
 */
void
tpb_raf_kernel_free_built_attr(kernel_attr_t *attr, void *data)
{
    if (attr != NULL && attr->headers != NULL) {
        tpb_raf_free_headers(attr->headers, attr->nheader);
        attr->headers = NULL;
    }
    free(data);
}
