# CREST接口使用说明
## 初始化与释放
crest在使用前需要进行初始化，使用完成后需要释放。初始化需要做一些创建线程池和工作线程的工作，使用接口如下
```c
int crest_global_init();
```
使用初始化的范例如下:
```c
int rc = crest_global_init();

if(rc != 0)
{
    // error process
    return -1;
}
```

在整个程序需要退出时，或者不再需要crest时，使用过如下接口释放
```c
void crest_global_uninit();
```
释放crest会阻塞当前线程，直到所有资源释放，工作者线程退出。

## 创建客户端与连接点
crest允许多个客户端实例，并采用是非面向连接的设计。因此，每一次rpc调用或者订阅某一个主题都需要显式地指定客户端句柄及服务器的连接点。我们使用下列接口创建和销毁客户端：
```c
crest_client_t crest_create_client();
void crest_release_client(crest_client_t client);
```
使用范例如下:
```c
crest_client_t client = crest_create_client();
if(NULL == client)
{
    // create client failed
}

// ....
// sometime later, when you wanna to exit or to simply delete the client entity

crest_release_client(client);

```
连接点同样需要创建和销毁，它实质上就是服务器的ip地址和端口号。我们使用如下接口创建和销毁连接点
```c
crest_endpoint_t crest_create_endpoint();
void crest_release_endpoint(crest_endpoint_t endpoint);
```
使用范例如下：
```c
crest_endpoint_t endpoint = crest_create_endpoint("127.0.0.1", 9000);
if(NULL == endpoint)
{
    // failed to create endpoint
}

// ....
crest_release_endpoint(endpoint);
```

## rpc调用
### 同步rpc调用
使用如下接口进行一次同步的rpc调用
```c
int crest_call(crest_call_param* call_param);
```
其中的参数结构体如下：
```c
typedef struct {
	char const*         topic;
	char const*         data;
	size_t              size;
} crest_request_t;

typedef struct {
	char*               data;
	size_t              size;
} crest_response_t;

typedef struct {
	crest_client_t 	    client;
	crest_endpoint_t    endpoint;
	crest_request_t	    request;
	crest_response_t    response;
	size_t              timeout;
} crest_call_param_t;
```
crest_request_t结构体是rpc调用需要向服务器发送的数据，crest_response_t是服务器返回的数据，用户在处理了这个结构体后，要自行清理其中的data的内存分配。同步rpc调用的范例如下:
```c
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
                // do the deserialization
		printf("Rpc call success\n");
	}

	// you have to clean the memory all by yourself
	if (NULL != sync_call_param.response.data)
		free(sync_call_param.response.data);
}
```
### 异步rpc调用
异步rpc调用使用如下接口:
```c
int crest_async_call(crest_call_async_param_t* call_param);
```
其相关结构体和回调函数定义如下：
```c
typedef void(*crest_on_recieve_message)(char const*, size_t);
typedef void(*crest_on_error)(int, char const*);

typedef struct {
	crest_client_t              client;
	crest_endpoint_t            endpoint;
	crest_request_t	            request;
	crest_on_recieve_message    on_recv;
	crest_on_error              on_error;
	size_t                      timeout;
} crest_call_async_param_t;
```
on_recv是rpc成功返回后的回调，而on_error是rpc发生错误的时候的回调。timeout是超时时间。使用范例如下：
```c
void async_recv_function(char const* data, size_t size)
{
	printf("success!\n");
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

	rc = crest_async_call(&call_param);
	if (rc != 0)
	{
		printf("rpc call failed!\n");
	}
}
```
用户不用操心回调函数中的数据生命周期，由crest自己维护。

### 订阅一个主题
订阅主题使用sub接口，其完整定义如下:
```c
typedef struct {
	crest_client_t              client;
	crest_endpoint_t            endpoint;
	char const*                 topic;
	crest_on_recieve_message    on_recv;
	crest_on_error              on_error;
} crest_recv_param_t;

int crest_async_recv(crest_recv_param_t* recv_param);
```
订阅主题只有异步接口，其的使用范例如下：
```c
void test_sub()
{
	crest_recv_param_t recv_param;
	memset(&recv_param, 0, sizeof(crest_recv_param_t));
	recv_param.client = client_handler;
	recv_param.endpoint = endpoint_handler;
	recv_param.on_error = error_function;
	recv_param.on_recv = sub_recv_function;
	recv_param.topic = "sub_add";

	int rc = crest_async_recv(&recv_param);
	if (0 != rc)
	{
		printf("sub failed!\n");
	}
}
```

## 尾巴
[仓库地址](https://github.com/IndignantAngel/crest)，接口不够用随时与我沟通，我会及时添加和维护bug.