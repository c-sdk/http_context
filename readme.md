# http_context

> [!WARNING]
> Still in development.

Types to create a HTTP request context.

These types are helpful for you to create a "request context" for your application.

```c
struct request_context_t {
  struct http_request_t request;
  struct http_response_t response;
  db_connection_t* database;
  void* data;
  // ...
}
```

## headers

Header names are matched case-insensitively. The same helpers work with request
and response headers:

```c
const char* content_type =
  http_headers_get(&request.headers, "Content-Type");

http_headers_put(&response.headers, "Cache-Control", "no-store");

if (http_headers_contains(&response.headers, "cache-control")) {
  http_headers_delete(&response.headers, "Cache-Control");
}
```

`http_headers_find` returns the mutable map entry when the key or value itself
needs to be inspected. `http_headers_put` borrows its key and value strings, so
they must remain valid while the headers are in use.

# dependencies

- pagesize
- arena
- string_map

# license

Unlicense.
