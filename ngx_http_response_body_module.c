#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


struct ngx_pair_s {
    ngx_str_t name;
    ngx_str_t value;
};
typedef struct ngx_pair_s ngx_pair_t;


typedef struct {
    ngx_msec_t   latency;
    ngx_pair_t  *headers_in;
    ngx_uint_t   headers_in_count;
    ngx_pair_t  *headers_out;
    ngx_uint_t   headers_out_count;
    ngx_uint_t  *statuses;
    ngx_uint_t   statuses_count;
    size_t       buffer_size;
    ngx_flag_t   capture_body;
} ngx_http_response_body_loc_conf_t;


typedef struct {
    ngx_http_response_body_loc_conf_t   *conf;
    ngx_buf_t                            buffer;
} ngx_http_response_body_ctx_t;


static ngx_int_t
ngx_http_response_body_add_variables(ngx_conf_t *cf);

static ngx_int_t
ngx_http_response_body_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

static void *ngx_http_response_body_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_response_body_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child);

static char *
ngx_http_response_body_statuses(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static char *
ngx_http_response_body_request_header_in(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);
static char *
ngx_http_response_body_response_header_in(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static ngx_int_t
ngx_http_response_body_set_ctx(ngx_http_request_t *r);

static ngx_int_t
ngx_http_response_body_log(ngx_http_request_t *r);

static ngx_int_t ngx_http_response_body_filter_header(ngx_http_request_t *r);
static ngx_int_t ngx_http_response_body_filter_body(ngx_http_request_t *r,
    ngx_chain_t *in);

static ngx_int_t ngx_http_response_body_init(ngx_conf_t *cf);


static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;


static ngx_int_t
ngx_http_next_header_filter_stub(ngx_http_request_t *r)
{
    return NGX_OK;
}


static ngx_int_t
ngx_http_next_body_filter_stub(ngx_http_request_t *r, ngx_chain_t *in)
{
    return NGX_OK;
}


static ngx_command_t  ngx_http_response_body_commands[] = {

    { ngx_string("capture_response_body"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_response_body_loc_conf_t, capture_body),
      NULL },

    { ngx_string("capture_response_body_if_request_header_in"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_response_body_request_header_in,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("capture_response_body_if_response_header_in"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_response_body_response_header_in,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("capture_response_body_if_status_in"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_response_body_statuses,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("capture_response_body_if_latency_more"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_response_body_loc_conf_t, latency),
      NULL },

    { ngx_string("capture_response_body_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_response_body_loc_conf_t, buffer_size),
      NULL },

      ngx_null_command

};


static ngx_http_module_t  ngx_http_response_body_module_ctx = {
    ngx_http_response_body_add_variables,    /* preconfiguration */
    ngx_http_response_body_init,             /* postconfiguration */

    NULL,                                    /* create main configuration */
    NULL,                                    /* init main configuration */

    NULL,                                    /* create server configuration */
    NULL,                                    /* merge server configuration */

    ngx_http_response_body_create_loc_conf,  /* create location configuration */
    ngx_http_response_body_merge_loc_conf    /* merge location configuration */
};


ngx_module_t  ngx_http_response_body_module = {
    NGX_MODULE_V1,
    &ngx_http_response_body_module_ctx,    /* module context */
    ngx_http_response_body_commands,       /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_http_variable_t  ngx_http_upstream_vars[] = {

    { ngx_string("response_body"), NULL,
      ngx_http_response_body_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_null_string, NULL, NULL, 0, 0, 0 }

};


static ngx_int_t
ngx_http_response_body_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var, *v;

    for (v = ngx_http_upstream_vars; v->name.len; v++) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_response_body_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_response_body_ctx_t *ctx;
    ngx_buf_t                    *b;

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    ctx = ngx_http_get_module_ctx(r, ngx_http_response_body_module);
    if (ctx == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    b = &ctx->buffer;

    if (b->start == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->data = b->start;
    v->len = b->last - b->start;

    return NGX_OK;
}


static char *
ngx_http_response_body_request_header_in(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                         *value;
    ngx_http_response_body_loc_conf_t *ulcf;
    char                              *sep;
    ngx_uint_t                         i;

    ulcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_response_body_module);
    if (ulcf == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    ulcf->headers_in_count = cf->args->nelts - 1;
    ulcf->headers_in = ngx_pcalloc(cf->pool, ulcf->headers_in_count * sizeof(ngx_pair_t));

    if (ulcf->headers_in == NULL)
    {
        return NULL;
    }

    for (i = 1; i < cf->args->nelts; ++i)
    {
        sep = ngx_strchr(value[i].data, '=');
        if (sep == NULL)
        {
            goto invalid_check_parameter;
        }
        
        ulcf->headers_in[i-1].name.len = (u_char *) sep - value[i].data;
        ulcf->headers_in[i-1].name.data = value[i].data;

        ulcf->headers_in[i-1].value.len = (ngx_uint_t) ((value[i].data + value[i].len - (u_char *) sep) - 1);
        ulcf->headers_in[i-1].value.data = (u_char *) sep + 1;
    }

    return NGX_CONF_OK;

invalid_check_parameter:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid capture_response_body_if_request_header_in \"%V\"", &value[i]);

    return NGX_CONF_ERROR;
}


static char *
ngx_http_response_body_response_header_in(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                         *value;
    ngx_http_response_body_loc_conf_t *ulcf;
    char                              *sep;
    ngx_uint_t                         i;

    ulcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_response_body_module);
    if (ulcf == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    ulcf->headers_out_count = cf->args->nelts - 1;
    ulcf->headers_out = ngx_pcalloc(cf->pool, ulcf->headers_out_count * sizeof(ngx_pair_t));

    if (ulcf->headers_out == NULL)
    {
        return NULL;
    }

    for (i = 1; i < cf->args->nelts; ++i)
    {
        sep = ngx_strchr(value[i].data, '=');
        if (sep == NULL)
        {
            goto invalid_check_parameter;
        }
        
        ulcf->headers_out[i-1].name.len = (u_char *) sep - value[i].data;
        ulcf->headers_out[i-1].name.data = value[i].data;

        ulcf->headers_out[i-1].value.len = (ngx_uint_t) ((value[i].data + value[i].len - (u_char *) sep) - 1);
        ulcf->headers_out[i-1].value.data = (u_char *) sep + 1;
    }

    return NGX_CONF_OK;

invalid_check_parameter:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid capture_response_body_if_response_header_in \"%V\"", &value[i]);

    return NGX_CONF_ERROR;
}


static char *
ngx_http_response_body_statuses(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                         *value;
    ngx_http_response_body_loc_conf_t *ulcf;
    ngx_uint_t                         i;

    ulcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_response_body_module);
    if (ulcf == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    ulcf->statuses_count = cf->args->nelts - 1;
    ulcf->statuses = ngx_pcalloc(cf->pool, ulcf->statuses_count * sizeof(ngx_uint_t));

    if (ulcf->statuses == NULL)
    {
        return NULL;
    }

    for (i = 1; i < cf->args->nelts; ++i)
    {
        ulcf->statuses[i-1] = ngx_atoi(value[i].data, value[i].len);
        if (ulcf->statuses[i-1] == (ngx_uint_t) NGX_ERROR || ulcf->statuses[i-1] == 0) {
            goto invalid_check_parameter;
        }
    }

    return NGX_CONF_OK;

invalid_check_parameter:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid capture_response_body_if_status_in \"%V\"", &value[i]);

    return NGX_CONF_ERROR;
}


static void *
ngx_http_response_body_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_response_body_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_response_body_loc_conf_t));

    if (conf == NULL) {
        return NULL;
    }

    conf->latency = NGX_CONF_UNSET_MSEC;
    conf->buffer_size = NGX_CONF_UNSET_SIZE;
    conf->headers_in = NGX_CONF_UNSET_PTR;
    conf->headers_in_count = NGX_CONF_UNSET_UINT;
    conf->headers_out = NGX_CONF_UNSET_PTR;
    conf->headers_out_count = NGX_CONF_UNSET_UINT;
    conf->statuses = NGX_CONF_UNSET_PTR;
    conf->statuses_count = NGX_CONF_UNSET_UINT;
    conf->capture_body = NGX_CONF_UNSET;

    return conf;
}


static char *
ngx_http_response_body_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_response_body_loc_conf_t *prev = parent;
    ngx_http_response_body_loc_conf_t *conf = child;

    ngx_conf_merge_msec_value(conf->latency, prev->latency, (ngx_msec_int_t) 0);
    ngx_conf_merge_size_value(conf->buffer_size, prev->buffer_size, (size_t) ngx_pagesize);
    ngx_conf_merge_ptr_value(conf->headers_in, prev->headers_in, NULL);
    ngx_conf_merge_uint_value(conf->headers_in_count, prev->headers_in_count, 0);
    ngx_conf_merge_ptr_value(conf->headers_out, prev->headers_out, NULL);
    ngx_conf_merge_uint_value(conf->headers_out_count, prev->headers_out_count, 0);
    ngx_conf_merge_ptr_value(conf->statuses, prev->statuses, NULL);
    ngx_conf_merge_uint_value(conf->statuses_count, prev->statuses_count, 0);
    ngx_conf_merge_value(conf->capture_body, prev->capture_body, 0);

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_response_body_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_response_body_set_ctx;

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_LOG_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_response_body_log;

    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_response_body_filter_header;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_response_body_filter_body;

    if (ngx_http_next_header_filter == NULL) {
        ngx_http_next_header_filter = ngx_http_next_header_filter_stub;
    }

    if (ngx_http_next_body_filter == NULL) {
        ngx_http_next_body_filter = ngx_http_next_body_filter_stub;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_response_body_set_ctx(ngx_http_request_t *r)
{
    ngx_http_response_body_loc_conf_t *ulcf;
    ngx_http_response_body_ctx_t *ctx;

    ulcf = ngx_http_get_module_loc_conf(r, ngx_http_response_body_module);

    if (!ulcf->capture_body) {
        return NGX_OK;
    }

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_response_body_ctx_t));
    if (ctx == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ctx->conf = ulcf;

    ngx_http_set_ctx(r, ctx, ngx_http_response_body_module);

    return NGX_OK;
}


static ngx_int_t
ngx_http_response_body_log(ngx_http_request_t *r)
{
    return NGX_OK;
}


static ngx_msec_t
ngx_http_response_body_request_time(ngx_http_request_t *r)
{
    ngx_time_t      *tp;
    ngx_msec_int_t   ms;

    tp = ngx_timeofday();

    ms = (ngx_msec_int_t)
             ((tp->sec - r->start_sec) * 1000 + (tp->msec - r->start_msec));

    return (ngx_msec_t) ngx_max(ms, 0);
}


static ngx_int_t
ngx_http_response_body_filter_header(ngx_http_request_t *r)
{
    ngx_http_response_body_ctx_t *ctx;
    ngx_uint_t                    status;
    ngx_uint_t                    i, j;
    ngx_list_part_t              *part;
    ngx_table_elt_t              *header;
    ngx_str_t                    *name;
    ngx_str_t                    *value;

    ctx = ngx_http_get_module_ctx(r, ngx_http_response_body_module);
    if (ctx == NULL) {
        return ngx_http_next_header_filter(r);
    }

    if (ctx->conf->headers_in_count == 0 &&
        ctx->conf->headers_out_count == 0 &&
        ctx->conf->statuses_count == 0 &&
        ctx->conf->latency == 0) {
        return ngx_http_next_header_filter(r);
    }

    if (ctx->conf->statuses_count != 0) {
        status = r->headers_out.status;
        for (i = 0; i < ctx->conf->statuses_count; ++i) {
            if (status == ctx->conf->statuses[i]) {
                return ngx_http_next_header_filter(r);
            }
        }
    }

    if (ctx->conf->latency != 0 && ctx->conf->latency <= ngx_http_response_body_request_time(r)) {
        return ngx_http_next_header_filter(r);
    }

    if (ctx->conf->headers_in_count != 0) {
        part = &r->headers_in.headers.part;
        header = part->elts;

        for (i = 0;; ++i) {
            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }

                part = part->next;
                header = part->elts;
                i = 0;
            }

            if (header[i].hash == 0) {
                continue;
            }

            for (j = 0; j < ctx->conf->headers_in_count; ++j) {
                name = &ctx->conf->headers_in[j].name;

                if (header[i].key.len != name->len) {
                    continue;
                }

                if (ngx_strncasecmp(header[i].key.data, name->data, name->len) != 0) {
                    continue;
                }

                value = &ctx->conf->headers_in[j].value;

                if (value->len == 0) {
                    return ngx_http_next_header_filter(r);
                }

                if (header[i].value.len != value->len) {
                    continue;
                }

                if (ngx_strncasecmp(header[i].value.data, value->data, value->len) == 0) {
                    return ngx_http_next_header_filter(r);
                }
            }
        }
    }

    if (ctx->conf->headers_out_count != 0) {
        part = &r->headers_out.headers.part;
        header = part->elts;

        for (i = 0;; ++i) {
            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }

                part = part->next;
                header = part->elts;
                i = 0;
            }

            if (header[i].hash == 0) {
                continue;
            }

            for (j = 0; j < ctx->conf->headers_out_count; ++j) {
                name = &ctx->conf->headers_out[j].name;

                if (header[i].key.len != name->len) {
                    continue;
                }

                if (ngx_strncasecmp(header[i].key.data, name->data, name->len) != 0) {
                    continue;
                }

                value = &ctx->conf->headers_out[j].value;

                if (value->len == 0) {
                    return ngx_http_next_header_filter(r);
                }

                if (header[i].value.len != value->len) {
                    continue;
                }

                if (ngx_strncasecmp(header[i].value.data, value->data, value->len) == 0) {
                    return ngx_http_next_header_filter(r);
                }
            }
        }
    }

    ngx_http_set_ctx(r, NULL, ngx_http_response_body_module);

    return ngx_http_next_header_filter(r);
}


static ngx_int_t
ngx_http_response_body_filter_body(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_http_response_body_ctx_t      *ctx;
    ngx_chain_t                       *cl;
    ngx_buf_t                         *b;
    ngx_http_response_body_loc_conf_t *conf;
    size_t                             len;
    ssize_t                            rest;

    ctx = ngx_http_get_module_ctx(r, ngx_http_response_body_module);
    if (ctx == NULL) {
        return ngx_http_next_body_filter(r, in);
    }

    conf = ngx_http_get_module_loc_conf(r, ngx_http_response_body_module);

    b = &ctx->buffer;

    if (b->start == NULL) {
        b->start = ngx_palloc(r->pool, conf->buffer_size);
        if (b->start == NULL) {
            return NGX_ERROR;
        }

        b->end = b->start + conf->buffer_size;
        b->pos = b->last = b->start;
    }

    for (cl = in; cl; cl = cl->next) {
        rest = b->end - b->last;
        if (rest == 0) {
            break;
        }

        if (!ngx_buf_in_memory(cl->buf)) {
            continue;
        }

        len = cl->buf->last - cl->buf->pos;

        if (len == 0) {
            continue;
        }

        if (len > (size_t) rest) {
            /* we truncate the exceeding part of the response body */
            len = rest;
        }

        b->last = ngx_copy(b->last, cl->buf->pos, len);
    }

    return ngx_http_next_body_filter(r, in);
}
