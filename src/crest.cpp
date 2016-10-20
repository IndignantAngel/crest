#include <rest_rpc/client.hpp>
#include "../include/crest.h"

namespace crest
{
	using codec_policy = timax::rpc::msgpack_codec;
	using buffer_t = codec_policy::buffer_type;
	using client_private_t = timax::rpc::async_client_private<codec_policy>;
	using endpoint_t = boost::asio::ip::tcp::endpoint;
	std::unique_ptr<timax::rpc::io_service_pool> pool;
}

int crest_global_init()
{
	try
	{
		crest::pool.reset(new timax::rpc::io_service_pool{ std::thread::hardware_concurrency() });
		crest::pool->start();
		return 0;
	}
	catch (...)
	{
		return -1;
	}
}

void crest_global_uninit()
{
	if (nullptr != crest::pool)
	{
		crest::pool->stop();
		crest::pool.reset();
	}
}

crest_client_t crest_create_client()
{
	try
	{
		auto client = new crest::client_private_t{ crest::pool->get_io_service() };
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
		delete reinterpret_cast<crest::client_private_t*>(client);
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

	auto client = reinterpret_cast<crest::client_private_t*>(call_param->client);
	if (nullptr == client)
		return -1;

	auto endpoint = reinterpret_cast<crest::endpoint_t*>(call_param->endpoint);
	if (nullptr == endpoint)
		return -1;

	if (call_param->request.data == nullptr)
		return -1;

	// create topic and send buffer
	std::string topic = call_param->request.topic;
	crest::buffer_t buffer{ call_param->request.data,
		call_param->request.data + call_param->request.size };

	// call the rpc
	auto ctx = timax::rpc::make_rpc_context(client->get_io_service(), 
		*endpoint, topic, crest::codec_policy{}, std::move(buffer));
	timax::rpc::rpc_task<crest::codec_policy> task{ *client, ctx };
	task.do_call_and_wait();

	// handle error
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
	
	auto client = reinterpret_cast<crest::client_private_t*>(call_param->client);
	if (nullptr == client)
		return -1;
	
	auto endpoint = reinterpret_cast<crest::endpoint_t*>(call_param->endpoint);
	if (nullptr == endpoint)
		return -1;
	
	if (call_param->request.data == nullptr)
		return -1;
	
	// create topic and send buffer
	std::string topic = call_param->request.topic;
	crest::buffer_t buffer{ call_param->request.data,
		call_param->request.data + call_param->request.size };
	
	{
		auto ctx = timax::rpc::make_rpc_context(client->get_io_service(),
			*endpoint, topic, crest::codec_policy{}, std::move(buffer));

		timax::rpc::rpc_task<crest::codec_policy> task{ *client, ctx };
	
		if (nullptr != call_param->on_recv)
		{
			task.ctx_->on_ok = [on_recv = call_param->on_recv]
				(char const* data, size_t size)
			{
				on_recv(data, size);
			};
		}
		
		if (nullptr != call_param->on_error)
			task.ctx_->on_error = [e = call_param->on_error](timax::rpc::exception const& error)
		{
			e(static_cast<int>(error.get_error_code()), error.get_error_message().c_str());
		};
	
		//task.ctx_->timeout = call_param->timeout == 0 ?
		//	std::chrono::microseconds::max() : std::chrono::microseconds(call_param->timeout);
	}
	
	return 0;
}

int crest_async_recv(crest_recv_param_t* recv_param)
{
	if (nullptr == recv_param)
		return -1;
	
	auto client = reinterpret_cast<crest::client_private_t*>(recv_param->client);
	if (nullptr == client)
		return -1;
	
	auto endpoint = reinterpret_cast<crest::endpoint_t*>(recv_param->endpoint);
	if (nullptr == endpoint)
		return -1;

	if (nullptr == recv_param->topic)
		return -1;

	auto& manager = client->get_sub_manager();
	
	// serialize topic name
	std::string topic = recv_param->topic;
	auto topic_buffer = crest::codec_policy{}.pack_args(topic);
	
	// create function
	std::function<void(char const*, size_t)> func = nullptr;
	if (nullptr != recv_param->on_recv)
	{
		func = [on_recv = recv_param->on_recv](char const* data, size_t size)
		{
			if (0 != on_recv(data, size))
			{
				throw timax::rpc::exception{ timax::rpc::error_code::CANCEL, "canceled by client error." };
			}
		};
	}

	// create error function
	std::function<void(timax::rpc::exception const&)> error_func = nullptr;
	if (nullptr != recv_param->on_error)
	{
		error_func = [on_error = recv_param->on_error](timax::rpc::exception const& error)
		{
			on_error(static_cast<int>(error.get_error_code()), error.get_error_message().c_str());
		};
	}
	
	auto sub_channel = std::make_shared<timax::rpc::sub_channel>(client->get_io_service(), 
		*endpoint, topic, topic_buffer, std::move(func), std::move(error_func));

	try
	{
		manager.sub_impl(*endpoint, topic, sub_channel);
	}
	catch (timax::rpc::exception const&)
	{
		return -1;
	}
	
	return 0;
}