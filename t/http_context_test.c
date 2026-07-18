#include <assert.h>
#include <string.h>

#include "http_context.h"
#include "http_methods.h"
#include "http_statuses.h"

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
  assert(strcmp(http_headers_get(&request.headers, "host"), "example.com") == 0);
  assert(strcmp(http_headers_get(&request.headers, "content-length"), "5") == 0);
  assert(strcmp(http_headers_get(&request.headers, "x-trimmed"), "spaced value") == 0);

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
  assert(strcmp(http_headers_get(&request.headers, "HOST"), "Example.com") == 0);
  assert(strcmp(http_headers_get(&request.headers, "X-Custom-Header"), "value") == 0);

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
  http_response_set_header(&response, "x-TEST", "two");
  assert(response.headers.count == 1);
  assert(strcmp(http_headers_get(&response.headers, "x-test"), "two") == 0);

  http_response_set_text(&response, "hello");
  assert(response.status == HTTP_STATUS_200);
  assert(strcmp(response.content, "hello") == 0);
  assert(strcmp(http_headers_get(&response.headers, "content-length"), "5") == 0);
  assert(strcmp(http_headers_get(&response.headers, "content-type"), "text/plain; charset=utf-8") == 0);

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
  assert(strcmp(http_headers_get(&response.headers, "location"), "/next") == 0);
  assert(strcmp(http_headers_get(&response.headers, "content-length"), "0") == 0);

  arena_free(&arena);
}

static void test_header_helpers_are_case_insensitive_and_exact(void) {
  arena_t arena = {0};
  assert(arena_create(&arena, 4096) == 0);

  struct string_map_t headers;
  string_map_init_with_arena(&arena, &headers, 8);

  assert(http_headers_put(&headers, "Host", "example.com") == 0);
  assert(http_headers_put(&headers, "Hostname", "server-1") == 0);
  assert(http_headers_contains(&headers, "hOsT"));
  assert(strcmp(http_headers_get(&headers, "HOST"), "example.com") == 0);
  assert(strcmp(http_headers_get(&headers, "hostname"), "server-1") == 0);
  assert(http_headers_get(&headers, "Hos") == NULL);
  assert(http_headers_find(&headers, "host") == &headers.data[0]);

  assert(http_headers_put(&headers, "HOST", "example.org") == 0);
  assert(headers.count == 2);
  assert(strcmp(http_headers_get(&headers, "host"), "example.org") == 0);
  assert(strcmp(headers.data[0].key, "Host") == 0);

  assert(http_headers_delete(&headers, "HOST") == 1);
  assert(!http_headers_contains(&headers, "host"));
  assert(strcmp(http_headers_get(&headers, "hostname"), "server-1") == 0);
  assert(http_headers_delete(&headers, "missing") == 0);

  arena_free(&arena);
}

static void test_header_put_removes_case_variant_duplicates(void) {
  arena_t arena = {0};
  assert(arena_create(&arena, 4096) == 0);

  struct string_map_t headers;
  string_map_init_with_arena(&arena, &headers, 4);
  assert(string_map_add(&headers, "X-Test", "one") == 0);
  assert(string_map_add(&headers, "x-test", "two") == 0);
  assert(string_map_add(&headers, "Other", "kept") == 0);

  assert(http_headers_put(&headers, "X-TEST", "three") == 0);
  assert(headers.count == 2);
  assert(strcmp(http_headers_get(&headers, "x-test"), "three") == 0);
  assert(strcmp(http_headers_get(&headers, "other"), "kept") == 0);

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
  test_header_helpers_are_case_insensitive_and_exact();
  test_header_put_removes_case_variant_duplicates();
  return 0;
}
