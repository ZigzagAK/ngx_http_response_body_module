Name
====

ngx_http_response_body_module - extract body response into variable.

Table of Contents
=================

* [Name](#name)
* [Status](#status)
* [Synopsis](#synopsis)
* [Description](#description)
* [Configuration directives](#configuration-directives)

Status
======

This library is under development.

Description
===========

Capture response body into nginx $response_body variable.

[Back to TOC](#table-of-contents)

Synopsis
========

```nginx
http {
    include       mime.types;
    default_type  application/octet-stream;

    log_format  main  '$remote_addr - $remote_user [$time_local] "$request" $status "$err" "$response_body"';

    access_log  logs/access.log  main;

    upstream test {
        server 127.0.0.1:9999;
    }

    capture_response_body on;
    capture_response_body_buffer_size 1m;
    capture_response_body_if_status_in 500 401 403 404;
    capture_response_body_if_latency_more 1s;

    map $response_body $err {
      ~\"error\":\"(?<e>.+)\" $e;
      default "";
    }

    server {
        listen 9999;
        location / {
            content_by_lua_block {
               for i=1,10000
               do
                 ngx.print("0000000000")
               end
               ngx.sleep(1.5)
            }
        }
        location /500 {
            content_by_lua_block {
               ngx.status = ngx.HTTP_INTERNAL_SERVER_ERROR;
               ngx.print('{"error":"internal error"}')
            }
        }
    }

    server {
        listen 8888;
        location / {
            proxy_pass http://test;
        }
    }
}```

[Back to TOC](#table-of-contents)

Configuration directives
========================

capture_response_body
--------------
* **syntax**: `capture_response_body on|off`
* **default**: `off`
* **context**: `http,server,location`

Turn on response body capture.

capture_response_body_buffer_size
--------------
* **syntax**: `capture_response_body_buffer_size <size>`
* **default**: `error`
* **context**: `http,server,location`

Maximum buffer size.

capture_response_body_if_status_in
--------------
* **syntax**: `capture_response_body_if_status_in <status1> <status2> ...`
* **default**: `none`
* **context**: `http,server,location`

Capture response body only for specific http statuses.

capture_response_body_if_latency_more
--------------
* **syntax**: `capture_response_body_if_latency_more <sec>`
* **default**: `none`
* **context**: `http,server,location`

Capture response body only if request time is greather than specified in the parameter.

[Back to TOC](#table-of-contents)