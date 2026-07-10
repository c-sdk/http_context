#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "http_headers.h"
#include "http_methods.h"
#include "http_statuses.h"
#include "http_context.h"
#include "pagesize.h"
#include "arena.h"


#define HTTP_HEADERS_DEFAULT_SIZE 24
#define HTTP_COOKIES_DEFAULT_SIZE 24
#define HTTP_DATA_DEFAULT_SIZE 24

static const char* http_trim_line_end(const char* start, const char* end) {
  if (end > start && end[-1] == '\r') {
    return end - 1;
  }
  return end;
}

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

int http_parse_request(arena_t* arena,
                       struct http_request_t* request,
                       const char *request_text) {
  if (arena == NULL || request == NULL || request_text == NULL) {
    return -1;
  }

  const char* line_end = strchr(request_text, '\n');
  if (line_end == NULL) {
    return -1;
  }

  const char* line_stop = http_trim_line_end(request_text, line_end);
  const char* position = request_text;

  const char* method_end = memchr(position, ' ', (size_t)(line_stop - position));
  if (method_end == NULL || method_end == position) {
    return -1;
  }
  request->method = arena_string_with_null(arena, position,
                                           (method_end - position + 1));

  position = method_end + 1;
  const char* path_end = memchr(position, ' ', (size_t)(line_stop - position));
  if (path_end == NULL || path_end == position) {
    return -1;
  }
  request->path = arena_string_with_null(arena, position,
                                         (path_end - position + 1));

  position = path_end + 1;
  if (position >= line_stop) {
    return -1;
  }
  request->version = arena_string_with_null(arena, position,
                                            (line_stop - position + 1));

  position = line_end + 1;

  while (position[0] != '\0') {
    line_end = strchr(position, '\n');
    if (line_end == NULL) {
      return -1;
    }

    line_stop = http_trim_line_end(position, line_end);
    if (line_stop == position) {
      position = line_end + 1;
      break;
    }

    const char* separator = memchr(position, ':', (size_t)(line_stop - position));
    if (separator == NULL || separator == position) {
      return -1;
    }

    const char* key = arena_string_with_null(arena, position,
                                             (separator - position + 1));

    const char* value_position = separator + 1;
    while (value_position < line_stop &&
           (*value_position == ' ' || *value_position == '\t')) {
      value_position++;
    }

    const char* value_end = line_stop;
    while (value_end > value_position &&
           (value_end[-1] == ' ' || value_end[-1] == '\t')) {
      value_end--;
    }

    const char* value = arena_string_with_null(arena, value_position,
                                               (value_end - value_position + 1));
    string_map_add(&request->headers, key, (void*)value);
    position = line_end + 1;
  }

  struct string_map_entry_t *contentLenghtHeader = NULL;
  const char* const kContentLengthHttpHeaderKey = "Content-Length";
  (void)string_map_find_by_key(&request->headers,
                               &contentLenghtHeader,
                               kContentLengthHttpHeaderKey);

  if (contentLenghtHeader) {
    char* endptr = NULL;
    long contentLength = strtol(contentLenghtHeader->value, &endptr, 10);
    if (endptr == contentLenghtHeader->value || contentLength < 0) {
      return -1;
    }
    request->content = arena_string_with_null(arena, position,
                                              (contentLength + 1));
  }

  return 0;
}

int http_request_is_get(const struct http_request_t *request) {
  return request != NULL && request->method != NULL && strcmp(request->method, GET) == 0;
}
int http_request_is_post(const struct http_request_t *request) {
  return request != NULL && request->method != NULL && strcmp(request->method, POST) == 0;
}

#define HTTP_RESPONSE_NAMES_SIZE 7
struct http_response_names_t {
  int number;
  int has_content;
  const char* const name;
} http_names[HTTP_RESPONSE_NAMES_SIZE] = {
  { HTTP_STATUS_200, 1, HTTP_STATUS_OK },
  { HTTP_STATUS_303, 0, HTTP_STATUS_SEE_OTHER },
  { HTTP_STATUS_400, 0, HTTP_STATUS_BAD_REQUEST },
  { HTTP_STATUS_401, 0, HTTP_STATUS_UNAUTHORIZED },
  { HTTP_STATUS_403, 0, HTTP_STATUS_FORBIDDEN },
  { HTTP_STATUS_404, 0, HTTP_STATUS_NOT_FOUND },
  { HTTP_STATUS_500, 0, HTTP_STATUS_INTERNAL_SERVER_ERROR }
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

  if (response->content != NULL) {
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
  response->status = HTTP_STATUS_200;
  response->content = (char*)content;

  char* content_length_value = arena_string_from_int(arena, strlen(content));
  string_map_add(&response->headers, "Content-Length", content_length_value);
  string_map_add(&response->headers, "Content-Type", "text/html");
}

void http_see_other(arena_t* arena,
                    struct http_response_t *response,
                    const char* const location) {
  (void)arena;
  response->status = HTTP_STATUS_303;
  string_map_add(&response->headers, "Content-Length", "0");
  string_map_add(&response->headers, "Location", (char*)location);
}
