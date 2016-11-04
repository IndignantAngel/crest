#include <crest.h>
#include <stdlib.h>
#include <msgpack.h>
#include <stdio.h>

#define ADD_RPC "add"
#define SUB_ADD "sub_add"

crest_client_t		client_handler;
crest_endpoint_t		endpoint_handler;

void test_sync_call()
{
	int rc;
	msgpack_sbuffer sbuf;
	msgpack_sbuffer_init(&sbuf);

	msgpack_packer pk;
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	// serialize 1 and 2
	msgpack_pack_array(&pk, 2);
	msgpack_pack_int(&pk, 1);
	msgpack_pack_int(&pk, 2);

	crest_call_param_t sync_call_param;
	memset(&sync_call_param, 0, sizeof(crest_call_param_t));
	sync_call_param.client = client_handler;
	sync_call_param.endpoint = endpoint_handler;
	sync_call_param.request.topic = ADD_RPC;
	sync_call_param.request.data = sbuf.data;
	sync_call_param.request.size = sbuf.size;

	rc = crest_call(&sync_call_param);
	if (rc != 0)
	{
		printf("Rpc call failed!\n");
	}
	else
	{
		printf("Rpc call success\n");
	}

	// you have to clean the memory all by yourself
	if (NULL != sync_call_param.response.data)
		free(sync_call_param.response.data);
}

void test_sync_pub()
{
	int rc;
	msgpack_sbuffer sbuf;
	msgpack_sbuffer_init(&sbuf);

	msgpack_packer pk;
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	// serialize 1 and 2
	msgpack_pack_array(&pk, 2);
	msgpack_pack_int(&pk, 1);

	crest_call_param_t sync_call_param;
	memset(&sync_call_param, 0, sizeof(crest_call_param_t));
	sync_call_param.client = client_handler;
	sync_call_param.endpoint = endpoint_handler;
	sync_call_param.request.topic = SUB_ADD;
	sync_call_param.request.data = sbuf.data;
	sync_call_param.request.size = sbuf.size;

	rc = crest_pub(&sync_call_param);
	if (rc != 0)
	{
		printf("Rpc call failed!\n");
	}
	else
	{
		printf("Rpc call success\n");
	}

	// you have to clean the memory all by yourself
	if (NULL != sync_call_param.response.data)
		free(sync_call_param.response.data);
}

int async_recv_function(char const* data, size_t size)
{
	printf("success!\n");
	return 0;
}

void error_function(int ec, char const* message)
{
	printf("error code: %d, error message: %s.\n", ec, message);
}

void test_async_call()
{
	int rc;
	msgpack_sbuffer sbuf;
	msgpack_sbuffer_init(&sbuf);

	msgpack_packer pk;
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	// serialize 1 and 2
	msgpack_pack_array(&pk, 2);
	msgpack_pack_int(&pk, 1);
	msgpack_pack_int(&pk, 2);

	crest_call_async_param_t call_param;
	memset(&call_param, 0, sizeof(crest_call_async_param_t));
	call_param.client = client_handler;
	call_param.endpoint = endpoint_handler;
	call_param.request.topic = ADD_RPC;
	call_param.request.size = sbuf.size;
	call_param.request.data = sbuf.data;
	call_param.on_recv = async_recv_function;
	call_param.on_error = error_function;
	call_param.timeout = 1000;

	rc = crest_async_call(&call_param);
	if (rc != 0)
	{
		printf("rpc call failed!\n");
	}
}

int sub_recv_function(char const* data, size_t size)
{
	printf("sub recv\n");
	return 0;
}

void test_sub()
{
	crest_sub_param_t recv_param;
	memset(&recv_param, 0, sizeof(crest_sub_param_t));
	recv_param.client = client_handler;
	recv_param.endpoint = endpoint_handler;
	recv_param.on_error = error_function;
	recv_param.on_recv = sub_recv_function;
	recv_param.topic = "sub_add";

	int rc = crest_async_sub(&recv_param);
	if (0 != rc)
	{
		printf("sub failed!\n");
	}
}

int main(int argc, char* argv[])
{
	int rc;
	rc = crest_global_init();
	if (rc != 0)
		return -1;

	client_handler = crest_create_client();
	if (NULL == client_handler)
		return -1;

	endpoint_handler = crest_create_endpoint("127.0.0.1", 9000);
	if (NULL == endpoint_handler)
		return -1;

	//test_sync_call();
	//test_async_call();
	test_sync_pub();
	//test_sub();

	getchar();
	crest_release_endpoint(endpoint_handler);
	crest_release_client(client_handler);
	crest_global_uninit();
	return 0;
}