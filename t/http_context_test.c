#include <assert.h>
#include <string.h>

#include "http_context.h"
#include "http_methods.h"
#include "http_statuses.h"

static const char* get_header(const struct http_request_t* request, const char* key) {
  struct string_map_entry_t* entry = NULL;
  (void)string_map_find_by_key((struct string_map_t*)&request->headers, &entry, key);
  return entry == NULL ? NULL : (const char*)entry->value;
}

static const char* get_response_header(const struct http_response_t* response, const char* key) {
  struct string_map_entry_t* entry = NULL;
  (void)string_map_find_by_key((struct string_map_t*)&response->headers, &entry, key);
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
  assert(strcmp(get_header(&request, "host"), "example.com") == 0);
  assert(strcmp(get_header(&request, "content-length"), "5") == 0);
  assert(strcmp(get_header(&request, "x-trimmed"), "spaced value") == 0);

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

static void test_parse_request_lowercases_header_keys(void) {
  arena_t arena = {0};
  assert(arena_create(&arena, 4096) == 0);

  struct http_request_t request;
  http_request_init(&arena, &request);

  const char* request_text =
    "GET / HTTP/1.1\r\n"
    "HoSt: Example.com\r\n"
    "X-CuStOm-HeAdEr: value\r\n"
    "\r\n";

  assert(http_parse_request(&arena, &request, request_text) == 0);
  assert(strcmp(get_header(&request, "host"), "Example.com") == 0);
  assert(strcmp(get_header(&request, "x-custom-header"), "value") == 0);

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

static void test_response_helpers_set_status_headers_and_body(void) {
  arena_t arena = {0};
  assert(arena_create(&arena, 4096) == 0);

  struct http_response_t response;
  http_response_init(&arena, &response);

  http_response_set_status(&response, HTTP_STATUS_404);
  assert(response.status == HTTP_STATUS_404);

  http_response_set_header(&response, "X-Test", "one");
  http_response_set_header(&response, "X-Test", "two");
  assert(strcmp(get_response_header(&response, "X-Test"), "two") == 0);

  http_response_set_text(&response, "hello");
  assert(response.status == HTTP_STATUS_200);
  assert(strcmp(response.content, "hello") == 0);
  assert(strcmp(get_response_header(&response, "Content-Length"), "5") == 0);
  assert(strcmp(get_response_header(&response, "Content-Type"), "text/plain; charset=utf-8") == 0);

  arena_free(&arena);
}

static void test_response_redirect_sets_location_and_empty_body(void) {
  arena_t arena = {0};
  assert(arena_create(&arena, 4096) == 0);

  struct http_response_t response;
  http_response_init(&arena, &response);

  http_response_set_redirect(&response, "/next");
  assert(response.status == HTTP_STATUS_303);
  assert(response.content == NULL);
  assert(strcmp(get_response_header(&response, "Location"), "/next") == 0);
  assert(strcmp(get_response_header(&response, "Content-Length"), "0") == 0);

  arena_free(&arena);
}

int main(void) {
  test_parse_request_with_body_and_trimmed_headers();
  test_parse_request_rejects_missing_request_delimiters();
  test_parse_request_rejects_headers_without_colons();
  test_parse_request_rejects_invalid_content_length();
  test_parse_request_lowercases_header_keys();
  test_request_method_helpers_use_exact_matches();
  test_response_helpers_set_status_headers_and_body();
  test_response_redirect_sets_location_and_empty_body();
  return 0;
}
