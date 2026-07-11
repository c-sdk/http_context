#include <assert.h>
#include <string.h>

#include "http_context.h"
#include "http_methods.h"

static const char* get_header(const struct http_request_t* request, const char* key) {
  struct string_map_entry_t* entry = NULL;
  (void)string_map_find_by_key((struct string_map_t*)&request->headers, &entry, key);
  return entry == NULL ? NULL : (const char*)entry->value;
}

static void test_parse_request_with_body_and_trimmed_headers(void) {
  arena_t arena = {0};
  assert(arena_create(&arena, 4096) == 0);

  struct http_request_t request;
  http_request_init(&arena, &request);

  const char* request_text =
    "POST /submit HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "Content-Length: 5\r\n"
    "X-Trimmed:   spaced value   \r\n"
    "\r\n"
    "hello";

  assert(http_parse_request(&arena, &request, request_text) == 0);
  assert(strcmp(request.method, HTTP_METHODS_POST) == 0);
  assert(strcmp(request.path, "/submit") == 0);
  assert(strcmp(request.version, "HTTP/1.1") == 0);
  assert(strcmp(request.content, "hello") == 0);
  assert(strcmp(get_header(&request, "Host"), "example.com") == 0);
  assert(strcmp(get_header(&request, "Content-Length"), "5") == 0);
  assert(strcmp(get_header(&request, "X-Trimmed"), "spaced value") == 0);

  arena_free(&arena);
}

static void test_parse_request_rejects_missing_request_delimiters(void) {
  arena_t arena = {0};
  assert(arena_create(&arena, 4096) == 0);

  struct http_request_t request;
  http_request_init(&arena, &request);

  const char* request_text =
    "GET/submit HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "\r\n";

  assert(http_parse_request(&arena, &request, request_text) == -1);

  arena_free(&arena);
}

static void test_parse_request_rejects_headers_without_colons(void) {
  arena_t arena = {0};
  assert(arena_create(&arena, 4096) == 0);

  struct http_request_t request;
  http_request_init(&arena, &request);

  const char* request_text =
    "GET / HTTP/1.1\r\n"
    "Broken-Header\r\n"
    "\r\n";

  assert(http_parse_request(&arena, &request, request_text) == -1);

  arena_free(&arena);
}

static void test_parse_request_rejects_invalid_content_length(void) {
  arena_t arena = {0};
  assert(arena_create(&arena, 4096) == 0);

  struct http_request_t request;
  http_request_init(&arena, &request);

  const char* request_text =
    "POST /submit HTTP/1.1\r\n"
    "Content-Length: not-a-number\r\n"
    "\r\n"
    "hello";

  assert(http_parse_request(&arena, &request, request_text) == -1);

  arena_free(&arena);
}

static void test_request_method_helpers_use_exact_matches(void) {
  struct http_request_t request = {0};

  request.method = HTTP_METHODS_GET;
  assert(http_request_is_get(&request));
  assert(!http_request_is_post(&request));

  request.method = "GETX";
  assert(!http_request_is_get(&request));

  request.method = HTTP_METHODS_POST;
  assert(http_request_is_post(&request));
  assert(!http_request_is_get(&request));
}

int main(void) {
  test_parse_request_with_body_and_trimmed_headers();
  test_parse_request_rejects_missing_request_delimiters();
  test_parse_request_rejects_headers_without_colons();
  test_parse_request_rejects_invalid_content_length();
  test_request_method_helpers_use_exact_matches();
  return 0;
}
