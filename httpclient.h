#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H
//---------------------------------------------------------------------------------------

#include <memory>
#include <cstddef>
#include <optional>
#include <string_view>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
//---------------------------------------------------------------------------------------

///< BOOST BEAST FORWARD DECLARATIONS
namespace boost::beast::http
{
	template<bool isRequest> class basic_parser;
	class message_generator;
}

namespace Stream::Http
{
	enum class ProtocolType { http, https };

    struct Endpoint {
        uint16_t port;
        std::string host;
    };

    struct ConnectedEndpoints
    {
        Endpoint local;
        Endpoint remote;
    };

	class HttpStream;

	class HttpClient
	{
	private:

		std::unique_ptr<HttpStream> pimpl;

	public:

		~HttpClient();
		explicit HttpClient(ProtocolType protocol, bool throws = false);
		
		HttpClient(HttpClient &&other) noexcept;
		HttpClient& operator=(HttpClient &&other) noexcept;

		HttpClient(const HttpClient&) = delete;
		HttpClient& operator=(const HttpClient&) = delete;

		///< Versions that do not throw an exception. They are deterministic.
		bool IsConnected() noexcept;
		void Close(boost::system::error_code &ec);
        ConnectedEndpoints GetEndpoints(boost::system::error_code &ec);
		std::size_t Write(boost::beast::http::message_generator &&msg, boost::system::error_code &ec);
		std::size_t Read(boost::beast::http::basic_parser<false> &parser, boost::system::error_code &ec);
		void Connect(std::string_view host, std::string_view port, boost::system::error_code &ec);
		void Connect(const boost::asio::ip::tcp::resolver::results_type &endpoints, boost::system::error_code &ec);

		///< Versions that throw an exception. They are non-deterministic.
		#ifndef BOOST_NO_EXCEPTIONS
			void Close();
            ConnectedEndpoints GetEndpoints();
			std::size_t Write(boost::beast::http::message_generator &&msg);
			std::size_t Read(boost::beast::http::basic_parser<false> &parser);
			void Connect(std::string_view host, std::string_view port);
			void Connect(const boost::asio::ip::tcp::resolver::results_type& endpoints);
		#endif
	};
}
//---------------------------------------------------------------------------------------
#endif
