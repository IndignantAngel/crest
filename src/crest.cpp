#include <rest_rpc/client.hpp>
#include "../include/crest.h"

namespace crest
{
	using client_t = timax::rpc::async_client<timax::rpc::msgpack_codec>;
	using endpoint_t = boost::asio::ip::tcp::endpoint;
}

crest_client_t crest_create_client()
{
	try
	{
		auto client = new crest::client_t{};
		return client;
	}
	catch (...)
	{
		return nullptr;
	}
}

void crest_release_client(crest_client_t client)
{
	if (nullptr != client)
		delete reinterpret_cast<crest::client_t*>(client);
}

crest_endpoint_t crest_create_endpoint(char const* addr, unsigned short port)
{
	try
	{
		auto endpoint = new crest::endpoint_t{
			boost::asio::ip::address::from_string(addr), port };
		return endpoint;
	}
	catch (...)
	{
		return nullptr;
	}
}

void crest_release_endpoint(crest_endpoint_t endpoint)
{
	if (nullptr != endpoint)
		delete reinterpret_cast<crest::endpoint_t*>(endpoint);
}

// TODO timeout
int crest_call(crest_call_param_t* call_param)
{
	if (nullptr == call_param)
		return -1;

	auto client = reinterpret_cast<crest::client_t*>(call_param->client);
	if (nullptr == client)
		return -1;

	auto endpoint = reinterpret_cast<crest::endpoint_t*>(call_param->endpoint);
	if (nullptr == endpoint)
		return -1;

	if (call_param->request.data == nullptr)
		return -1;

	// create topic and send buffer
	std::string topic = call_param->request.topic;
	std::vector<char> buffer{ call_param->request.data,
		call_param->request.data + call_param->request.size };

	// call the rpc
	crest::client_t::rpc_task_base task(*client, *endpoint, topic, std::move(buffer));
	task.do_call_and_wait();

	auto code = task.ctx_->err.get_error_code();

	// get the result
	if (code == timax::rpc::error_code::OK)
	{
		auto& response = task.ctx_->rep;
		call_param->response.data = reinterpret_cast<char*>(std::malloc(response.size()));
		std::memcpy(call_param->response.data, response.data(), response.size());
	}
	
	return static_cast<int>(code);
}

int crest_async_call(crest_call_async_param_t* call_param)
{
	if (nullptr == call_param)
		return -1;

	auto client = reinterpret_cast<crest::client_t*>(call_param->client);
	if (nullptr == client)
		return -1;

	auto endpoint = reinterpret_cast<crest::endpoint_t*>(call_param->endpoint);
	if (nullptr == endpoint)
		return -1;

	if (call_param->request.data == nullptr)
		return -1;

	// create topic and send buffer
	std::string topic = call_param->request.topic;
	std::vector<char> buffer{ call_param->request.data,
		call_param->request.data + call_param->request.size };

	{
		crest::client_t::rpc_task_base task(*client, *endpoint, topic, std::move(buffer));

		if (nullptr != call_param->on_recv)
			task.ctx_->on_ok = call_param->on_recv;

		if (nullptr != call_param->on_error)
			task.ctx_->on_error = [e = call_param->on_error](timax::rpc::exception const& error)
		{
			e(static_cast<int>(error.get_error_code()), error.get_error_message().c_str());
		};

		task.ctx_->timeout = call_param->timeout == 0 ?
			std::chrono::microseconds::max() : std::chrono::microseconds(call_param->timeout);
	}
	
	return 0;
}

int crest_async_recv(crest_recv_param_t* recv_param)
{
	if (nullptr == recv_param)
		return -1;

	auto client = reinterpret_cast<crest::client_t*>(recv_param->client);
	if (nullptr == client)
		return -1;

	auto endpoint = reinterpret_cast<crest::endpoint_t*>(recv_param->endpoint);
	if (nullptr == endpoint)
		return -1;

	//client->sub()
	return 0;
}