#include <rest_rpc/client.hpp>
#include "../include/crest.h"

namespace crest
{
	using codec_policy = timax::rpc::msgpack_codec;
	using buffer_t = codec_policy::buffer_type;
	using client_private_t = timax::rpc::async_client_private<codec_policy>;
	using endpoint_t = boost::asio::ip::tcp::endpoint;
	std::unique_ptr<timax::rpc::io_service_pool> pool;
	using context_ptr = std::shared_ptr<timax::rpc::rpc_context<codec_policy>>;
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

int crest_sync_impl(crest_call_param_t* param, bool is_pub)
{
	if (nullptr == param)
		return -1;

	auto crest_client = reinterpret_cast<crest::client_private_t*>(param->client);
	if (nullptr == crest_client)
		return -1;

	auto crest_endpoint = reinterpret_cast<crest::endpoint_t*>(param->endpoint);
	if (nullptr == crest_endpoint)
		return -1;

	auto& request = param->request;
	auto& response = param->response;

	if (!(nullptr != request.data && 0 != request.size && nullptr != request.topic))
		return -1;

	std::string topic = request.topic;
	uint64_t hash = is_pub ? crest_client->hash(timax::rpc::PUB) : crest_client->hash(topic);
	crest::buffer_t buffer{ request.data, request.data + request.size };

	crest::context_ptr ctx;
	if (is_pub)
	{
		ctx = timax::rpc::make_rpc_context<crest::codec_policy>(
			crest_client->get_io_service(), *crest_endpoint, hash, topic, std::move(buffer));
	}
	else
	{
		ctx = timax::rpc::make_rpc_context<crest::codec_policy>(
			crest_client->get_io_service(), *crest_endpoint, hash, std::move(buffer));
	}

	ctx->timeout = 0 == param->timeout ? std::chrono::nanoseconds::max() :
		std::chrono::microseconds(param->timeout);

	if (nullptr == ctx)
		return -1;

	timax::rpc::rpc_task<crest::codec_policy> task{ *crest_client, ctx };
	try
	{
		task.do_call_and_wait();
	}
	catch (timax::rpc::exception const& error)
	{
		auto code = static_cast<int>(error.get_error_code());
		auto& rep = task.ctx_->rep;
		response.data = reinterpret_cast<char*>(std::malloc(rep.size()));
		std::memcpy(response.data, rep.data(), rep.size());
		return code;
	}

	return 0;
}

int crest_call(crest_call_param_t* call_param)
{
	return crest_sync_impl(call_param, false);
}

int crest_pub(crest_call_param_t* call_param)
{
	return crest_sync_impl(call_param, true);
}

int crest_async_impl(crest_call_async_param_t* param, bool is_pub)
{
	if (nullptr == param)
		return -1;

	auto client = reinterpret_cast<crest::client_private_t*>(param->client);
	if (nullptr == client)
		return -1;

	auto endpoint = reinterpret_cast<crest::endpoint_t*>(param->endpoint);
	if (nullptr == endpoint)
		return -1;

	auto& request = param->request;
	if (!(nullptr != request.data && 0 != request.size && nullptr != request.topic))
		return -1;

	std::string topic = request.topic;
	uint64_t hash = is_pub ? client->hash(timax::rpc::PUB) : client->hash(topic);
	crest::buffer_t buffer{ request.data, request.data + request.size };

	// 233333333
	{
		// create rpc context
		crest::context_ptr ctx;
		if (is_pub)
		{
			ctx = timax::rpc::make_rpc_context<crest::codec_policy>(
				client->get_io_service(), *endpoint, hash, topic, std::move(buffer));
		}
		else
		{
			ctx = timax::rpc::make_rpc_context<crest::codec_policy>(
				client->get_io_service(), *endpoint, hash, std::move(buffer));
		}

		ctx->timeout = 0 == param->timeout ? std::chrono::nanoseconds::max() : 
			std::chrono::microseconds(param->timeout);

		// create rpc task to manage call process
		timax::rpc::rpc_task<crest::codec_policy> task{ *client, ctx };

		// async success process
		if (nullptr != param->on_recv)
		{
			task.ctx_->on_ok = [on_recv = param->on_recv]
			(char const* data, size_t size)
			{
				on_recv(data, size);
			};
		}

		// async error process
		if (nullptr != param->on_error)
			task.ctx_->on_error = [e = param->on_error](timax::rpc::exception const& error)
		{
			e(static_cast<int>(error.get_error_code()), error.get_error_message().c_str());
		};
	}	// call by the mechanism of RAII

	return 0;
}

int crest_async_call(crest_call_async_param_t* param)
{
	return crest_async_impl(param, false);
}

int crest_async_pub(crest_call_async_param_t* param)
{
	return crest_async_impl(param, true);
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