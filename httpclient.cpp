
#include <optional>
#include <functional>
#include <boost/beast/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/core.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/certify/extensions.hpp>
#include <boost/certify/https_verification.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "httpclient.h"
//-----------------------------------------------------------------------------

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http  = beast::http;  // from <boost/beast/http.hpp>
namespace net   = boost::asio;  // from <boost/asio.hpp>
namespace ssl   = net::ssl;     // from <boost/asio/ssl.hpp>
using     tcp   = net::ip::tcp; // from <boost/asio/ip/tcp.hpp>
//-----------------------------------------------------------------------------

namespace Stream::Http
{
	class HttpStream
	{
	protected:

		net::io_context ioc;
		beast::flat_buffer buffer;

	public:

		virtual ~HttpStream() = default;
		virtual bool IsConnected() noexcept = 0;
		virtual void Close(beast::error_code &ec) = 0;
        virtual ConnectedEndpoints GetEndpoints(beast::error_code &ec) = 0;
		virtual std::size_t Write(http::message_generator&& msg, beast::error_code &ec) = 0;
		virtual std::size_t Read(http::basic_parser<false> &parser, beast::error_code &ec) = 0;
		virtual void Connect(std::string_view host, std::string_view port, beast::error_code &ec) = 0;
		virtual void Connect(const tcp::resolver::results_type& endpoints, beast::error_code &ec) = 0;
	};

	class PlainHttpStream : public HttpStream
	{
	private:

		std::unique_ptr<beast::tcp_stream> stream;

		std::optional<beast::error_code>
		PrepareConnection(std::optional<std::string_view> host, std::optional<std::string_view> port,
						  std::optional<std::reference_wrapper<const tcp::resolver::results_type>> endpoints)
		{
			stream = std::unique_ptr<beast::tcp_stream>(new (std::nothrow) beast::tcp_stream(ioc));

			buffer.clear();
			if (!stream)
				return boost::system::errc::make_error_code(boost::system::errc::not_enough_memory);

			beast::error_code ec;

			try
			{
				tcp::resolver::results_type localEndpoints;

				while (true)
				{
					if (endpoints.has_value())
						localEndpoints = endpoints->get();
					else
					{
						tcp::resolver resolver(ioc);
						localEndpoints = resolver.resolve(host.value(), port.value(), ec);

						if (ec)
							break;
					}

					stream->connect(localEndpoints, ec);
					if (ec)
						break;

					return std::nullopt;
				}
			}
			catch (const boost::system::system_error& ecode) { ec = ecode.code(); }
			catch (...) { ec = boost::system::errc::make_error_code(boost::system::errc::protocol_error); }

			{
				beast::error_code errc;
				Close(errc);
			}

			return ec;
		}

	public:

		bool IsConnected() noexcept override
		{
			// Check if the object exists
			if (!stream) return false;
			// Check if socket is open local.
			if (!stream->socket().is_open()) return false;
			// Check if the connection was closed by the remote host
			beast::error_code ec;
			stream->socket().available(ec);

			return !ec;
		}

		void Close(beast::error_code &ec) override
		{
			beast::error_code ec_shutdown;
			beast::error_code ec_close;

			if (stream)
			{
				try
				{
					// Close Socket
					stream->socket().shutdown(tcp::socket::shutdown_both, ec_shutdown);
					// Check if error
					if (ec_shutdown == net::error::not_connected ||
						ec_shutdown == net::error::eof)
						ec_shutdown = {};

					// Close descriptor
					if (stream->socket().is_open())
						stream->socket().close(ec_close);
				}
				catch (...) { ec_close = boost::system::errc::make_error_code(boost::system::errc::io_error); }

				// Free smart pointer
				stream.reset();
			}

			// Set error acording to the priority.
			if   (ec_shutdown) ec = ec_shutdown;
			else if (ec_close) ec = ec_close;
		}

        ConnectedEndpoints GetEndpoints(beast::error_code &ec) override
        {
            if (!IsConnected())
                ec = boost::asio::error::not_connected;
            else
            {
                auto &asio_socket = stream->socket();

                // Get remote (server) data IP & Port
                boost::asio::ip::tcp::endpoint remote_ep = asio_socket.remote_endpoint(ec);
                if (ec)
                    return {};
                // Get local (local pc-server) data IP & Port
                boost::asio::ip::tcp::endpoint local_ep = asio_socket.local_endpoint(ec);
                if (ec)
                    return {};

                return {
                    { local_ep.port() , local_ep.address().to_string()  },
                    { remote_ep.port(), remote_ep.address().to_string() }
                };
            }

            return {};
        }

		std::size_t Write(http::message_generator &&msg, beast::error_code &ec) override
		{
			std::size_t bytes = 0;

			if (!IsConnected())
				ec = boost::asio::error::not_connected;
			else
			{
				try {
					bytes = beast::write(*stream, std::move(msg), ec);
				}
				catch (const boost::system::system_error& ecode) { ec = ecode.code(); }
				catch (...) { ec = boost::system::errc::make_error_code(boost::system::errc::io_error); }

				if (ec) {
					beast::error_code errc;
					Close(errc);
				}
			}

			return bytes;
		}

		std::size_t Read(http::basic_parser<false> &parser, beast::error_code &ec) override
		{
			std::size_t bytes = 0;

			if (!IsConnected())
				ec = boost::asio::error::not_connected;
			else
			{
				try {
					bytes = http::read(*stream, buffer, parser, ec);
				}
				catch (const boost::system::system_error& ecode) { ec = ecode.code(); }
				catch (...) { ec = boost::system::errc::make_error_code(boost::system::errc::io_error); }

				if (ec) {
					beast::error_code errc;
					Close(errc);
				}
			}

			return bytes;
		}

		void Connect(std::string_view host, std::string_view port, beast::error_code &ec) override
		{
			if (!IsConnected())
			{
				if (host.empty() || port.empty())
					ec = net::error::host_not_found;
				else
					ec = PrepareConnection(host, port, {}).value_or(beast::error_code{});
			}
		}

		void Connect(const tcp::resolver::results_type& endpoints, beast::error_code &ec) override
		{
			if (!IsConnected())
			{
				if (endpoints.empty())
					ec = net::error::host_not_found;
				else
					ec = PrepareConnection({}, {}, endpoints).value_or(beast::error_code{});
			}
		}
	};

	class SecureHttpStream : public HttpStream
	{
	private:

		std::unique_ptr<ssl::context> ctx;
		std::unique_ptr<beast::ssl_stream <beast::tcp_stream>> stream;

		std::optional<beast::error_code>
		PrepareConnection(std::string_view host, std::optional<std::string_view> port,
                          std::optional<std::reference_wrapper<const tcp::resolver::results_type>> endpoints)
		{
			beast::error_code ec;

			try
			{
				buffer.clear();
				if (!ctx)
				{
					auto sslCtx = std::unique_ptr<ssl::context>(new (std::nothrow) ssl::context(ssl::context::tlsv13_client));

					if (!sslCtx)
						return boost::system::errc::make_error_code(boost::system::errc::not_enough_memory);
					
					sslCtx->set_verify_mode(ssl::context::verify_peer | ssl::context::verify_fail_if_no_peer_cert, ec);
					if (ec)
						return ec;
					
					sslCtx->set_default_verify_paths(ec);
					if (ec)
						return ec;

					boost::certify::enable_native_https_server_verification(*sslCtx);
					ctx = std::move(sslCtx);
				}

				stream = std::unique_ptr<beast::ssl_stream<beast::tcp_stream>>(new (std::nothrow) beast::ssl_stream<beast::tcp_stream>(ioc, *ctx));

				if (!stream)
					return boost::system::errc::make_error_code(boost::system::errc::not_enough_memory);
				if (!SSL_set_tlsext_host_name(stream->native_handle(), std::string(host).c_str()))
					ec = { static_cast<int>(::ERR_get_error()), net::error::get_ssl_category() };
				else
				{
					tcp::resolver::results_type localEndpoints;

					while (true)
					{
						if (endpoints.has_value())
							localEndpoints = endpoints->get();
						else
						{
							tcp::resolver resolver(ioc);
							localEndpoints = resolver.resolve(host, port.value(), ec);

							if (ec)
								break;
						}

						beast::get_lowest_layer(*stream).connect(localEndpoints, ec);       // Make the connection on the IP address we get from a lookup
						if (ec)
							break;

						stream->handshake(ssl::stream_base::client, ec);                    // Perform the SSL handshake
						if (ec)
							break;

						return std::nullopt;
					}
				}
			}
			catch (const boost::system::system_error &ecode) { ec = ecode.code(); }
			catch (...) { ec = boost::system::errc::make_error_code(boost::system::errc::protocol_error); }

			{
				beast::error_code errc;
				Close(errc);
			}

			return ec;
		}

	public:

		bool IsConnected() noexcept override
		{
			// Check if the object exists
			if (!stream) return false;
			// Check if socket is open local.
			if (!boost::beast::get_lowest_layer(*stream).socket().is_open()) return false;
			// Check if the connection was closed by the remote host
			beast::error_code ec;
			boost::beast::get_lowest_layer(*stream).socket().available(ec);

			return !ec;
		}

		void Close(beast::error_code &ec) override
		{
			beast::error_code ec_tls;
			beast::error_code ec_tcp;
			beast::error_code ec_close;

			if (stream)
			{
				try
				{
					// TLS Shutdown 
					stream->shutdown(ec_tls);
					if (ec_tls == net::error::eof ||
						ec_tls == net::ssl::error::stream_truncated ||
						ec_tls == net::error::not_connected)
						ec_tls = {};
					
					// TCP Shutdown
					boost::beast::get_lowest_layer(*stream).socket().shutdown(tcp::socket::shutdown_both, ec_tcp);
					if (ec_tcp == net::error::not_connected)
						ec_tcp = {};

					// Close OS descriptor
					if (boost::beast::get_lowest_layer(*stream).socket().is_open())
						boost::beast::get_lowest_layer(*stream).socket().close(ec_close);
				}
				catch (...) { ec_close = boost::system::errc::make_error_code(boost::system::errc::io_error); }

				// Free smart pointer
				stream.reset();
			}

			// Set error acording to the priority.
			if      (ec_tls)   ec = ec_tls;
			else if (ec_tcp)   ec = ec_tcp;
			else if (ec_close) ec = ec_close;
		}

        ConnectedEndpoints GetEndpoints(beast::error_code &ec) override
        {
            if (!IsConnected())
                ec = boost::asio::error::not_connected;
            else
            {
                auto &asio_socket = boost::beast::get_lowest_layer(*stream).socket();

                // Get remote (server) data IP & Port
                boost::asio::ip::tcp::endpoint remote_ep = asio_socket.remote_endpoint(ec);
                if (ec)
                    return {};
                // Get local (local pc-server) data IP & Port
                boost::asio::ip::tcp::endpoint local_ep = asio_socket.local_endpoint(ec);
                if (ec)
                    return {};

                return {
                    { local_ep.port() , local_ep.address().to_string()  },
                    { remote_ep.port(), remote_ep.address().to_string() }
                };
            }

            return {};
        }

		std::size_t Write(http::message_generator &&msg, beast::error_code &ec) override
		{
			std::size_t bytes = 0;

			if (!IsConnected())
				ec = boost::asio::error::not_connected;
			else
			{
				try {
					bytes = beast::write(*stream, std::move(msg), ec);
				}
				catch (const boost::system::system_error& ecode) { ec = ecode.code(); }
				catch (...) { ec = boost::system::errc::make_error_code(boost::system::errc::protocol_error); }

				if (ec) {
					beast::error_code errc;
					Close(errc);
				}
			}

			return bytes;
		}

		std::size_t Read(http::basic_parser<false> &parser, beast::error_code &ec) override
		{
			std::size_t bytes = 0;

			if (!IsConnected())
				ec = boost::asio::error::not_connected;
			else
			{
				try {
					bytes = http::read(*stream, buffer, parser, ec);
				}
				catch (const boost::system::system_error& ecode) { ec = ecode.code(); }
				catch (...) { ec = boost::system::errc::make_error_code(boost::system::errc::protocol_error); }

				if (ec) {
					beast::error_code errc;
					Close(errc);
				}	
			}

			return bytes;
		}

		void Connect(std::string_view host, std::string_view port, beast::error_code &ec) override
		{
			if (!IsConnected())
			{
				if (host.empty() || port.empty())
					ec = net::error::host_not_found;
				else
					ec = PrepareConnection(host, port, {}).value_or(beast::error_code{});
			}
		}

		void Connect(const tcp::resolver::results_type& endpoints, beast::error_code &ec) override
		{
			if (!IsConnected())
			{
				std::string hostFromEP = (!endpoints.empty()) ? endpoints.begin()->host_name() : "";

				if (hostFromEP.empty() && !endpoints.empty())
					hostFromEP = endpoints.begin()->endpoint().address().to_string();
				if (hostFromEP.empty())
					ec = net::error::host_not_found;
				else
					ec = PrepareConnection(hostFromEP, {}, endpoints).value_or(beast::error_code{});
			}
		}
	};

	/*******************************************************************************
	*                                  HTTP CLIENT                                 *
	*                            PUBLIC IMPLEMENTATION                             *
	*******************************************************************************/

	HttpClient::HttpClient(HttpClient&& other) noexcept = default;
	HttpClient& HttpClient::operator=(HttpClient&& other) noexcept = default;

	HttpClient::HttpClient(ProtocolType protocol, bool throws)
	{
		if (protocol == ProtocolType::https)
			pimpl = std::unique_ptr<HttpStream>(new (std::nothrow) SecureHttpStream());
		else
			pimpl = std::unique_ptr<HttpStream>(new (std::nothrow) PlainHttpStream());

		if (throws && !pimpl)
			throw boost::system::system_error(boost::system::errc::make_error_code(boost::system::errc::not_enough_memory));
	}

	HttpClient::~HttpClient()
	{
		if (IsConnected()) {
			beast::error_code ec;
			this->Close(ec);
		}
	}

	bool HttpClient::IsConnected() noexcept
	{
		if (pimpl)
			return pimpl->IsConnected();
		else
			return false;
	}

	void HttpClient::Close(beast::error_code &ec)
	{
		ec.clear();
		if (pimpl)
			pimpl->Close(ec);
		else
			ec = boost::system::errc::make_error_code(boost::system::errc::not_enough_memory);
	}

    ConnectedEndpoints HttpClient::GetEndpoints(beast::error_code &ec)
    {
        ec.clear();
        if (pimpl)
            return pimpl->GetEndpoints(ec);
        else
            ec = boost::system::errc::make_error_code(boost::system::errc::not_enough_memory);

        return {};
    }

	std::size_t HttpClient::Write(boost::beast::http::message_generator &&msg, beast::error_code &ec)
	{
		std::size_t bytes = 0;

		ec.clear();
		if (pimpl)
			bytes = pimpl->Write(std::move(msg),ec);
		else
			ec = boost::system::errc::make_error_code(boost::system::errc::not_enough_memory);

		return bytes;
	}

	std::size_t HttpClient::Read(boost::beast::http::basic_parser<false> &parser, beast::error_code &ec)
	{
		std::size_t bytes = 0;

		ec.clear();
		if (pimpl)
			bytes = pimpl->Read(parser,ec);
		else
			ec = boost::system::errc::make_error_code(boost::system::errc::not_enough_memory);

		return bytes;
	}
	
	void HttpClient::Connect(std::string_view host, std::string_view port, beast::error_code &ec)
	{
		ec.clear();
		if (pimpl)
			pimpl->Connect(host, port, ec);
		else
			ec = boost::system::errc::make_error_code(boost::system::errc::not_enough_memory);
	}

	void HttpClient::Connect(const tcp::resolver::results_type& endpoints, beast::error_code &ec)
	{
		ec.clear();
		if (pimpl)
			pimpl->Connect(endpoints, ec);
		else
			ec = boost::system::errc::make_error_code(boost::system::errc::not_enough_memory);
	}

#ifndef BOOST_NO_EXCEPTIONS
	void HttpClient::Close()
	{
		beast::error_code ec;
		
		this->Close(ec);
		if (ec)
			throw boost::system::system_error(ec);
	}

    ConnectedEndpoints HttpClient::GetEndpoints()
    {
        beast::error_code ec;
        ConnectedEndpoints endpoints = this->GetEndpoints(ec);

        if (ec)
            throw boost::system::system_error(ec);

        return endpoints;
    }

	std::size_t HttpClient::Write(boost::beast::http::message_generator &&msg)
	{
		beast::error_code ec;
		std::size_t bytes = this->Write(std::move(msg), ec);
		
		if (ec)
			throw boost::system::system_error(ec);

		return bytes;
	}

	std::size_t HttpClient::Read(boost::beast::http::basic_parser<false> &parser)
	{
		beast::error_code ec;
		std::size_t bytes = this->Read(parser, ec);

		if (ec)
			throw boost::system::system_error(ec);

		return bytes;
	}

	void HttpClient::Connect(std::string_view host, std::string_view port)
	{
		beast::error_code ec;
		
		this->Connect(host, port, ec);
		if (ec)
			throw boost::system::system_error(ec);
	}

	void HttpClient::Connect(const boost::asio::ip::tcp::resolver::results_type& endpoints)
	{
		beast::error_code ec;

		this->Connect(endpoints, ec);
		if (ec)
			throw boost::system::system_error(ec);
	}
#endif
}