#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "http_context.h"
#include "pagesize.h"
#include "arena.h"


#define HTTP_HEADERS_DEFAULT_SIZE 24
#define HTTP_COOKIES_DEFAULT_SIZE 24
#define HTTP_DATA_DEFAULT_SIZE 24

void http_request_init(struct arena_t* arena,
                       struct http_request_t* request) {

  request->method = NULL;
  request->path = NULL;
  request->version = NULL;
  request->content = NULL;
  string_map_init_with_arena(arena, &request->headers, HTTP_HEADERS_DEFAULT_SIZE);
}

void http_response_init(struct arena_t* arena, struct http_response_t* response) {
  response->content = NULL;
  (void)string_map_init_with_arena(arena, &response->headers, HTTP_HEADERS_DEFAULT_SIZE);
}

void http_parse_request(arena_t* arena,
                        struct http_request_t* request,
                        const char *request_text) {
  const char* header_end = strchr((char*)request_text, '\n') - 1;
  char* position = (char*)request_text;

  const char* method_end = strchr(position, ' ');
  request->method = arena_string_with_null(arena, position, (method_end - position + 1));
  position = (char*)method_end + 1;

  const char* path_end = strchr(position, ' ');
  request->path = arena_string_with_null(arena, position, (path_end - position + 1));
  position = (char*)path_end + 1;

  request->version = arena_string_with_null(arena, position, (header_end - position  + 1));
  position = (char*)header_end + 2;

  while ((header_end = strchr((char*)position, '\n') - 1)) {
    if (header_end == position) { break; }
    const char* separator = strchr(position, ':');
    const char* key = arena_string_with_null(arena, position, (separator - position + 1));
    const char* value = arena_string_with_null(arena, separator + 2, (header_end - 2 - separator + 1));
    string_map_add(&request->headers, key, (void*)value);
    position = (char*)header_end + 2;
  }

  position = (char*)header_end + 2;

  struct string_map_entry_t *contentLenghtHeader = NULL;
  const char* const kContentLengthHttpHeaderKey = "Content-Length";
  (void)string_map_find_by_key(&request->headers,
                               &contentLenghtHeader,
                               kContentLengthHttpHeaderKey);

  if (contentLenghtHeader) {
    int contentLength = atoi(contentLenghtHeader->value);
    request->content = arena_string_with_null(arena, position,
                                              (contentLength + 1));
  }
}

int http_request_is_get(const struct http_request_t *request) {
  return memcmp(request->method, "GET", 3) == 0;
}
int http_request_is_post(const struct http_request_t *request) {
  return memcmp(request->method, "POST", 4) == 0;
}

#define HTTP_RESPONSE_NAMES_SIZE 7
struct http_response_names_t {
  int number;
  int has_content;
  const char* const name;
} http_names[HTTP_RESPONSE_NAMES_SIZE] = {
  { 200, 1, "OK" },
  { 303, 0, "See Other" },
  { 400, 0, "Bad Request" },
  { 401, 0, "Unauthorized" },
  { 403, 0, "Forbidden" },
  { 404, 0, "Not Found" },
  { 500, 0, "Internal Server Error" }
};

static const struct http_response_names_t* http_response_find_name_by_status(int number) {
  for (int i = 0; i < HTTP_RESPONSE_NAMES_SIZE; ++i) {
    struct http_response_names_t* item = &http_names[i];
    if (item->number == number) {
     return item;
    }
  }
  return NULL;
}

void http_send(int client_socket, struct http_response_t *response) {
  arena_t response_content = {0};
  int res = arena_create(&response_content, page_size());
  assert(res == 0);

  const struct http_response_names_t* r =
    http_response_find_name_by_status(response->status);

  int point = 0;
  point += snprintf(response_content.memory + point, page_size(),
           "HTTP/1.1 %d %s\r\n",
           r->number, r->name);

  for (size_t i = 0; i < response->headers.count; ++i) {
    struct string_map_entry_t* h = &response->headers.data[i];
    point += snprintf(response_content.memory + point, page_size(),
                      "%s: %s\r\n",
                      h->key, (char*)h->value);
  }

  if (r->has_content) {
    point += snprintf(response_content.memory + point, page_size(),
                      "\r\n%s",
                      response->content);
  } else {
    point += snprintf(response_content.memory + point, page_size(), "\r\n");
  }

  http_context_log("response:\n%s\n", response_content.memory);

  (void)write(client_socket, response_content.memory, point);

  arena_free(&response_content);
}

int http_read_request(int client_socket, char* buffer, size_t buffer_size) {
  ssize_t request_read = 0;
  size_t space_remaining = buffer_size;
  size_t buffer_read = 0;
  while ((request_read = read(client_socket, buffer + buffer_read, 1024)) > 0) {
    http_context_log("read socket status: %ld, %ld, %ld\n", request_read, buffer_read, space_remaining);
    space_remaining -= request_read;
    buffer_read += request_read;
    if (space_remaining < 0) {
      return -1;
    }
    if (request_read < 1024) {
      break;
    }
  }
  buffer[buffer_read++] = 0;

  http_context_log("incoming request:\n%s\n", buffer);

  return (buffer_read >= 0) - 1;
}

void http_ok_response(arena_t* arena,
                      struct http_response_t *response,
                      const char* const content) {
  response->status = 200;
  response->content = (char*)content;

  char* content_length_value = arena_string_from_int(arena, strlen(content));
  string_map_add(&response->headers, "Content-Length", content_length_value);
  string_map_add(&response->headers, "Content-Type", "text/html");
}

void http_see_other(arena_t* arena,
                    struct http_response_t *response,
                    const char* const location) {
  (void)arena;
  response->status = 303;
  string_map_add(&response->headers, "Content-Length", "0");
  string_map_add(&response->headers, "Location", (char*)location);
}
