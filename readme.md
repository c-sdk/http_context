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

# dependencies

- pagesize
- arena
- string_map

# license

Unlicense.
