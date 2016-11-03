#ifndef TIMAX_CREST_RPC_H
#define TIMAX_CREST_RPC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*crest_post_send_message)(int);
typedef int(*crest_on_recieve_message)(char const*, size_t);
typedef void(*crest_on_error)(int, char const*);

typedef void* crest_client_t;
typedef void* crest_endpoint_t;

int crest_global_init();
void crest_global_uninit();

crest_client_t crest_create_client();
void crest_release_client(crest_client_t server);

crest_endpoint_t crest_create_endpoint(char const* addr, unsigned short port);
void crest_release_endpoint(crest_endpoint_t endpoint);

typedef struct {
	char const*  topic;
	char const*  data;
	size_t		size;
} crest_request_t;

typedef struct {
	char* 		data;
	size_t 		size;
} crest_response_t;

typedef struct {
	crest_client_t 			client;
	crest_endpoint_t 			endpoint;
	crest_request_t			request;
	crest_response_t			response;
	size_t					timeout;
} crest_call_param_t;

int crest_call(crest_call_param_t* call_param);
int crest_pub(crest_call_param_t* pub_param);

typedef struct {
	crest_client_t			client;
	crest_endpoint_t			endpoint;
	crest_request_t			request;
	crest_on_recieve_message	on_recv;
	crest_on_error			on_error;
	size_t					timeout;
} crest_call_async_param_t;

int crest_async_call(crest_call_async_param_t* call_param);
int crest_async_pub(crest_call_async_param_t* pub_param);

typedef struct {
	crest_client_t			client;
	crest_endpoint_t			endpoint;
	char const* 				topic;
	crest_on_recieve_message	on_recv;
	crest_on_error			on_error;
} crest_sub_param_t;

int crest_async_sub(crest_sub_param_t* recv_param);

#ifdef __cplusplus
}
#endif

#endif