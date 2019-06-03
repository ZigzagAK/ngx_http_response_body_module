#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_msec_t    latency;
    ngx_flag_t    status_1xx;
    ngx_flag_t    status_2xx;
    ngx_flag_t    status_3xx;
    ngx_flag_t    status_4xx;
    ngx_flag_t    status_5xx;
    size_t        buffer_size;
    ngx_flag_t    capture_body;
    ngx_str_t     capture_body_var;
    ngx_array_t  *conditions;
    ngx_array_t  *cv;
} ngx_http_response_body_loc_conf_t;


typedef struct {
    ngx_http_response_body_loc_conf_t   *blcf;
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
ngx_http_response_body_request_var(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static ngx_int_t
ngx_http_response_body_set_ctx(ngx_http_request_t *r);

static ngx_int_t
ngx_http_response_body_log(ngx_http_request_t *r);

static ngx_int_t ngx_http_response_body_filter_header(ngx_http_request_t *r);
static ngx_int_t ngx_http_response_body_filter_body(ngx_http_request_t *r,
    ngx_chain_t *in);

static ngx_int_t ngx_http_response_body_init(ngx_conf_t *cf);


static char *
ngx_conf_set_flag(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static char *
ngx_conf_set_msec(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static char *
ngx_conf_set_size(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static char *
ngx_conf_set_keyval(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

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
      ngx_conf_set_flag,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_response_body_loc_conf_t, capture_body),
      NULL },

    { ngx_string("capture_response_body_var"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_response_body_request_var,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("capture_response_body_if"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
      ngx_conf_set_keyval,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_response_body_loc_conf_t, conditions),
      NULL },

    { ngx_string("capture_response_body_if_1xx"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_response_body_loc_conf_t, status_1xx),
      NULL },

    { ngx_string("capture_response_body_if_2xx"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_response_body_loc_conf_t, status_2xx),
      NULL },

    { ngx_string("capture_response_body_if_3xx"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_response_body_loc_conf_t, status_3xx),
      NULL },

    { ngx_string("capture_response_body_if_4xx"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_response_body_loc_conf_t, status_4xx),
      NULL },

    { ngx_string("capture_response_body_if_5xx"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_flag,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_response_body_loc_conf_t, status_5xx),
      NULL },

    { ngx_string("capture_response_body_if_latency_more"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_response_body_loc_conf_t, latency),
      NULL },

    { ngx_string("capture_response_body_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size,
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


static char *
ngx_conf_set_flag(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_response_body_loc_conf_t  *blcf = conf;
    char                               *p = conf;
    ngx_flag_t                         *fp = (ngx_flag_t *) (p + cmd->offset);
    ngx_flag_t                          prev = *fp;

    *fp = NGX_CONF_UNSET;

    if (ngx_conf_set_flag_slot(cf, cmd, conf) != NGX_CONF_OK)
        return NGX_CONF_ERROR;

    if (prev != NGX_CONF_UNSET)
        *fp = ngx_max(prev, *fp);

    blcf->capture_body = *fp;

    return NGX_CONF_OK;
}


char *
ngx_conf_set_msec(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_response_body_loc_conf_t  *blcf = conf;
    char                               *p = conf;
    ngx_msec_t                         *fp = (ngx_msec_t *) (p + cmd->offset);
    ngx_msec_t                          prev = *fp;

    *fp = NGX_CONF_UNSET_MSEC;

    if (ngx_conf_set_msec_slot(cf, cmd, conf) != NGX_CONF_OK)
        return NGX_CONF_ERROR;

    if (prev != NGX_CONF_UNSET_MSEC)
        *fp = ngx_min(prev, *fp);

    blcf->capture_body = 1;

    return NGX_CONF_OK;
}


static char *
ngx_conf_set_size(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char    *p = conf;
    size_t  *fp = (size_t *) (p + cmd->offset);
    size_t   prev = *fp;

    *fp = NGX_CONF_UNSET_SIZE;

    if (ngx_conf_set_size_slot(cf, cmd, conf) != NGX_CONF_OK)
        return NGX_CONF_ERROR;

    if (prev != NGX_CONF_UNSET_SIZE)
        *fp = ngx_max(prev, *fp);

    return NGX_CONF_OK;
}


static char *
ngx_conf_set_keyval(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_response_body_loc_conf_t  *blcf = conf;

    if (ngx_conf_set_keyval_slot(cf, cmd, conf) != NGX_CONF_OK)
        return NGX_CONF_ERROR;

    blcf->capture_body = 1;

    return NGX_CONF_OK;
}


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
ngx_http_response_body_request_var(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ngx_http_response_body_loc_conf_t *ulcf = conf;
    ngx_http_variable_t               *var;

    ulcf->capture_body_var = ((ngx_str_t *)cf->args->elts) [1];

    var = ngx_http_add_variable(cf, &ulcf->capture_body_var,
                                NGX_HTTP_VAR_NOCACHEABLE);
    if (var == NULL) {
        return NGX_CONF_ERROR;
    }

    var->get_handler = ngx_http_response_body_variable;
    var->data = 0;

    return NGX_CONF_OK;
}


static void *
ngx_http_response_body_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_response_body_loc_conf_t  *blcf;

    blcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_response_body_loc_conf_t));

    if (blcf == NULL)
        return NULL;

    blcf->latency      = NGX_CONF_UNSET_MSEC;
    blcf->buffer_size  = NGX_CONF_UNSET_SIZE;
    blcf->conditions   = ngx_array_create(cf->pool, 10, sizeof(ngx_keyval_t));
    blcf->cv           = ngx_array_create(cf->pool, 10,
        sizeof(ngx_http_complex_value_t));
    blcf->status_1xx   = NGX_CONF_UNSET;
    blcf->status_2xx   = NGX_CONF_UNSET;
    blcf->status_3xx   = NGX_CONF_UNSET;
    blcf->status_4xx   = NGX_CONF_UNSET;
    blcf->status_5xx   = NGX_CONF_UNSET;
    blcf->capture_body = NGX_CONF_UNSET;

    if (blcf->conditions == NULL || blcf->cv == NULL)
        return NULL;

    return blcf;
}


static ngx_int_t
ngx_array_merge(ngx_array_t *l, ngx_array_t *r)
{
    void  *p;

    if (r->nelts == 0)
        return NGX_OK;

    if (r->size != l->size)
        return NGX_ERROR;
    
    p = ngx_array_push_n(l, r->nelts);
    if (p == NULL)
        return NGX_ERROR;

    ngx_memcpy(p, r->elts, r->size * r->nelts);

    return NGX_OK;
}


static char *
ngx_http_response_body_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_response_body_loc_conf_t  *prev = parent;
    ngx_http_response_body_loc_conf_t  *conf = child;
    ngx_http_compile_complex_value_t    ccv;
    ngx_http_complex_value_t           *cv;
    ngx_uint_t                          j;
    ngx_keyval_t                       *kv;

    ngx_conf_merge_msec_value(conf->latency, prev->latency, (ngx_msec_int_t) 0);
    ngx_conf_merge_size_value(conf->buffer_size, prev->buffer_size,
                              (size_t) ngx_pagesize);
    if (ngx_array_merge(conf->conditions, prev->conditions) == NGX_ERROR)
        return NGX_CONF_ERROR;
    ngx_conf_merge_value(conf->status_1xx, prev->status_1xx, 0);
    ngx_conf_merge_value(conf->status_2xx, prev->status_2xx, 0);
    ngx_conf_merge_value(conf->status_3xx, prev->status_3xx, 0);
    ngx_conf_merge_value(conf->status_4xx, prev->status_4xx, 0);
    ngx_conf_merge_value(conf->status_5xx, prev->status_5xx, 0);
    ngx_conf_merge_value(conf->capture_body, prev->capture_body, 0);

    cv = ngx_array_push_n(conf->cv, conf->conditions->nelts);
    if (cv == NULL)
        return NGX_CONF_ERROR;
    ngx_memzero(cv, conf->cv->size * conf->cv->nalloc);
    kv = conf->conditions->elts;

    for (j = 0; j < conf->cv->nelts; j++) {

        ngx_memzero(&ccv, sizeof(ccv));

        ccv.cf = cf;
        ccv.value = &kv[j].key;
        ccv.complex_value = &cv[j];
        ccv.zero = 0;

        if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {

            ngx_conf_log_error(NGX_LOG_ERR, cf, 0,
                               "can't compile '%V'", &kv[j].key);
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_response_body_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_LOG_PHASE].handlers);
    if (h == NULL)
        return NGX_ERROR;

    *h = ngx_http_response_body_log;

    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_response_body_filter_header;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_response_body_filter_body;

    if (ngx_http_next_header_filter == NULL)
        ngx_http_next_header_filter = ngx_http_next_header_filter_stub;

    if (ngx_http_next_body_filter == NULL)
        ngx_http_next_body_filter = ngx_http_next_body_filter_stub;

    return NGX_OK;
}


static ngx_int_t
ngx_http_response_body_set_ctx(ngx_http_request_t *r)
{
    ngx_http_response_body_loc_conf_t  *ulcf;
    ngx_http_response_body_ctx_t       *ctx;

    ulcf = ngx_http_get_module_loc_conf(r, ngx_http_response_body_module);

    if (!ulcf->capture_body)
        return NGX_DECLINED;

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_response_body_ctx_t));
    if (ctx == NULL)
        return NGX_ERROR;

    ctx->blcf = ulcf;

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
    ngx_http_response_body_ctx_t  *ctx;
    ngx_uint_t                     j;
    ngx_http_complex_value_t      *cv;
    ngx_str_t                      value;
    ngx_keyval_t                  *kv;

    switch (ngx_http_response_body_set_ctx(r)) {
        case NGX_OK:
            break;

        case NGX_DECLINED:
            return ngx_http_next_header_filter(r);

        case NGX_ERROR:
            ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                "ngx_http_response_body_filter_header: no memory");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;

        default:
            return ngx_http_next_header_filter(r);
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_response_body_module);

    if (r->headers_out.status < 200) {

       if (ctx->blcf->status_1xx)
           return ngx_http_next_header_filter(r);

    } else if (r->headers_out.status < 300) {

        if (ctx->blcf->status_2xx)
           return ngx_http_next_header_filter(r);

    } else if (r->headers_out.status < 400) {

        if (ctx->blcf->status_3xx)
           return ngx_http_next_header_filter(r);

    } else if (r->headers_out.status < 500) {

        if (ctx->blcf->status_4xx)
           return ngx_http_next_header_filter(r);

    } else {

        if (ctx->blcf->status_5xx)
           return ngx_http_next_header_filter(r);

    }

    if (ctx->blcf->latency != 0
        && ctx->blcf->latency <= ngx_http_response_body_request_time(r))
        return ngx_http_next_header_filter(r);

    cv = ctx->blcf->cv->elts;
    kv = ctx->blcf->conditions->elts;

    for (j = 0; j < ctx->blcf->cv->nelts; ++j) {

        if (ngx_http_complex_value(r, &cv[j], &value) != NGX_OK)
            continue;

        if (value.len == 0)
            continue;

        if (kv[j].value.len == 0
            || (kv[j].value.len == 1 && kv[j].value.data[0] == '*'))
            return ngx_http_next_header_filter(r);

        if (kv[j].value.len == value.len
            && ngx_strncasecmp(value.data, kv[j].value.data, value.len) == 0)
            return ngx_http_next_header_filter(r);
    }

    ngx_http_set_ctx(r, NULL, ngx_http_response_body_module);

    return ngx_http_next_header_filter(r);
}


static ngx_int_t
ngx_http_response_body_filter_body(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_http_response_body_ctx_t       *ctx;
    ngx_chain_t                        *cl;
    ngx_buf_t                          *b;
    ngx_http_response_body_loc_conf_t  *conf;
    size_t                              len;
    ssize_t                             rest;

    ctx = ngx_http_get_module_ctx(r, ngx_http_response_body_module);
    if (ctx == NULL)
        return ngx_http_next_body_filter(r, in);

    conf = ngx_http_get_module_loc_conf(r, ngx_http_response_body_module);

    b = &ctx->buffer;

    if (b->start == NULL) {

        b->start = ngx_palloc(r->pool, conf->buffer_size);
        if (b->start == NULL)
            return NGX_ERROR;

        b->end = b->start + conf->buffer_size;
        b->pos = b->last = b->start;
    }

    for (cl = in; cl; cl = cl->next) {

        rest = b->end - b->last;
        if (rest == 0)
            break;

        if (!ngx_buf_in_memory(cl->buf))
            continue;

        len = cl->buf->last - cl->buf->pos;

        if (len == 0)
            continue;

        if (len > (size_t) rest)
            /* we truncate the exceeding part of the response body */
            len = rest;

        b->last = ngx_copy(b->last, cl->buf->pos, len);
    }

    return ngx_http_next_body_filter(r, in);
}
