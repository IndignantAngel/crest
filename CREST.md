# CREST接口使用说明
## 数据结构与回调函数
```cpp
// crest客户端实例指针
typedef void* crest_client_t;
```
```cpp
// crest连接点实例指针
typedef void* crest_endpoint_t;
```
```cpp
// crest发送请求结构体
typedef struct {
	char const*         topic;		// 请求的topic
	char const*         data;		// 请求数据的起始地址
	size_t              size;		// 请求的数据长度
} crest_request_t;
```
```cpp
// crest应答数据结构
typedef struct {
	char*               data;		// 应答数据的起始地址
	size_t              size;		// 应答数据的长度
} crest_response_t;
```
```cpp
// crest调用数据结构
typedef struct {
	crest_client_t 	    client;		// 客户端指针
	crest_endpoint_t    endpoint;		// 连接点指针
	crest_request_t	    request;		// 请求结构体
	crest_response_t    response;		// 应答结构体
	size_t              timeout;		// 超时时间，以毫秒为单位
} crest_call_param_t;
```
```cpp
// 收到应答时的回调函数，在异步接口中使用
// 返回值：在sub中有效，在call中无效。非零，退出订阅状态；零，维持订阅状态
// 参数1：char const*， 收到数据的起始地址
// 参数2：size_t，收到数据的长度
typedef int(*crest_on_recieve_message)(char const*, size_t);
```
```cpp
// 在使用crest异步接口中的错误回调
// 参数1: 错误码
// 参数2：错误消息字符串
typedef void(*crest_on_error)(int, char const*);
```
```cpp
// 异步接口的输入参数数据结构
typedef struct {
	crest_client_t              client;		// 客户端指针
	crest_endpoint_t            endpoint;		// 连接点指针 
	crest_request_t	            request;		// 请求结构体
	crest_on_recieve_message    on_recv;		// 接受消息回调函数
	crest_on_error              on_error;		// 接收消息错误的回调函数
	size_t                      timeout;		// 超时，以毫秒为单位
} crest_call_async_param_t;
```
```cpp
// 订阅主题的数据结构
typedef struct {
	crest_client_t 			client;		// 客户端指针
	crest_endpoint_t 		endpoint; 	// 连接点指针
	char const* 			topic;  	// 订阅的主题
	crest_on_recieve_message 	on_recv; 	// 接受消息回调函数
	crest_on_error 			on_error; 	// 接收消息错误的回调函数
} crest_recv_param_t;
```

## 应用程序接口说明
### 初始化与释放
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

### 创建客户端与连接点
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

### rpc调用
#### 同步rpc调用
使用如下接口进行一次同步的rpc调用
```c
int crest_call(crest_call_param* call_param);
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
#### 异步rpc调用
异步rpc调用使用如下接口:
```c
int crest_async_call(crest_call_async_param_t* call_param);
```
on_recv是rpc成功返回后的回调，而on_error是rpc发生错误的时候的回调。timeout是超时时间。使用范例如下：
```c
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

	rc = crest_async_call(&call_param);
	if (rc != 0)
	{
		printf("rpc call failed!\n");
	}
}
```
用户不用操心回调函数中的数据生命周期，由crest自己维护。receive函数的返回值在async call中可以忽略，它在下面的订阅发布中有重要的作用。

### 订阅一个主题
订阅主题使用sub接口，其完整定义如下:
```c
int crest_async_recv(crest_recv_param_t* recv_param);
```
订阅主题只有异步接口，其的使用范例如下：
```c
int sub_recv_function(char const* data, size_t size)
{
	printf("sub recv\n");
	return 0;

	// to stop, return a value that not equals to 0
}

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
sub_recv_function的返回值可以用来终止订阅的回调循环。如果返回一个非零值，订阅主题的回调函数退出后，不再会进入，并会终止这一次订阅。

## 尾巴
[仓库地址](https://github.com/IndignantAngel/crest)，接口不够用随时与我沟通，我会及时添加和维护bug.