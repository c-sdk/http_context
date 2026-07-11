#ifndef __AIL_HTTP_H__
#define __AIL_HTTP_H__ 1

#include "arena.h"
#include "string_map.h"

#ifdef LOG_HTTP_CONTEXT
#define http_context_log(...) printf(__VA_ARGS__)
#else
#define http_context_log(...)
#endif

struct http_request_t {
  char* method;
  char* path;
  char* content;
  char* version;
  struct string_map_t headers;
};

struct http_response_t {
  size_t status;
  char* content;
  struct string_map_t headers;
};

int http_parse_request(arena_t *arena, struct http_request_t *request,
                       const char *request_text);
int http_read_request(int client_socket, char* buffer, size_t buffer_size);

void http_request_init(struct arena_t *arena, struct http_request_t *request);
int http_request_is_get(const struct http_request_t *request);
int http_request_is_post(const struct http_request_t *request);

void http_response_init(struct arena_t* arena, struct http_response_t* response);
void http_send(int client_socket, struct http_response_t *response);

void http_response_set_status(struct http_response_t* response, size_t status);
void http_response_set_header(struct http_response_t* response,
                              const char* key,
                              const char* value);
void http_response_set_text(struct http_response_t* response,
                            const char* content);
void http_response_set_redirect(struct http_response_t* response,
                                const char* location);

#endif
