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

This library is production ready.

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
    log_format  test  '$remote_addr - $remote_user [$time_local] "$request" $status "$err" "$test_response_body"';


    access_log  logs/access.log  main;

    upstream test {
        server 127.0.0.1:7777;
    }

    map $status $cond {
      500       1;
      401       1;
      403       1;
      404       1;
      default   0;
    }

    capture_response_body                   off;
    capture_response_body_buffer_size       1m;
    capture_response_body_if                $cond 1;
    capture_response_body_if_latency_more   1s;

    map $response_body $err {
      ~\"error\":\"(?<e>.+)\" $e;
      default "";
    }

    server {
        listen 7777;
        location / {
            echo_sleep 1.5;
            echo '0000000000';
        }
        location /500 {
            echo_status 500;
            echo '{"error":"internal error"}';
        }
        location /200 {
            echo OK;
        }
        location /404 {
            echo_status 404;
            echo '404';
        }
        location /header_in {
            echo 'OK';
        }
        location /header_out {
            add_header X-Trace-Response 1;
            echo 'OK';
        }
        location /test {
            add_header X-Trace-Response 1;
            echo 'OK';
        }
    }

    server {
        capture_response_body on;
        listen 8888;
        location / {
            proxy_pass http://test;
        }
        location /header_in {
            capture_response_body_if $http_x_trace *;
            proxy_pass http://test;
        }
        location /header_out {
            capture_response_body_if $sent_http_x_trace_response *;
            proxy_pass http://test;
        }
        location /test {
            access_log  logs/test.log  test;
            capture_response_body_var test_response_body;
            capture_response_body_if $sent_http_x_trace_response 1;
            proxy_pass http://test;
        }
    }
}
```

[Back to TOC](#table-of-contents)

Configuration directives
========================

capture_response_body
--------------
* **syntax**: `capture_response_body on|off`
* **default**: `off`
* **context**: `http,server,location`

Turn on response body capture.

capture_response_body_var
--------------
* **syntax**: `capture_response_body_var <name>`
* **default**: `request_body`
* **context**: `http,server,location`

Variable name.

capture_response_body_buffer_size
--------------
* **syntax**: `capture_response_body_buffer_size <size>`
* **default**: `none`
* **context**: `http,server,location`

Maximum buffer size.

capture_response_body_if
--------------
* **syntax**: `capture_response_body_if <complex variable> <value>`
* **default**: `none`
* **context**: `http,server,location`

Capture response body if result of calculation <complex variable> is equal <value>.  
<value> may be empty string or special '*'.

capture_response_body_if_1xx
--------------
* **syntax**: `capture_response_body_if_1xx on`
* **default**: `off`
* **context**: `http,server,location`

Capture response body for http statuses 1xx.

capture_response_body_if_2xx
--------------
* **syntax**: `capture_response_body_if_2xx on`
* **default**: `off`
* **context**: `http,server,location`

Capture response body for http statuses 2xx.

capture_response_body_if_3xx
--------------
* **syntax**: `capture_response_body_if_3xx on`
* **default**: `off`
* **context**: `http,server,location`

Capture response body for http statuses 3xx.

capture_response_body_if_4xx
--------------
* **syntax**: `capture_response_body_if_4xx on`
* **default**: `off`
* **context**: `http,server,location`

Capture response body for http statuses 4xx.

capture_response_body_if_5xx
--------------
* **syntax**: `capture_response_body_if_5xx on`
* **default**: `off`
* **context**: `http,server,location`

Capture response body for http statuses 5xx.

capture_response_body_if_latency_more
--------------
* **syntax**: `capture_response_body_if_latency_more <sec>`
* **default**: `none`
* **context**: `http,server,location`

Capture response body only if request time is greather than specified in the parameter.

[Back to TOC](#table-of-contents)
