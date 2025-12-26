#include <boost/asio.hpp>
#include <ctime>
#include <iostream>
#include <memory>

namespace sys = boost::system;
namespace asio = boost::asio;
using udp = asio::ip::udp;

std::string
make_daytime_string ()
{
  time_t now = time (0);
  return ctime (&now);
}

class udp_server
{
private:
  udp::socket m_socket;
  udp::endpoint m_endpoint;
  std::array<char, 1> m_buffer;

public:
  udp_server (asio::io_context &ctx)
      : m_socket (ctx, udp::endpoint (udp::v4 (), 13))
  {
    start_receive ();
  }

  void
  start_receive ()
  {
    // m_socket.async_receive_from (
    //     asio::buffer (m_buffer), m_endpoint,
    //     std::bind (&udp_server::handle_receive, this,
    //     	   asio::placeholders::error,
    //     	   asio::placeholders::bytes_transferred));
    m_socket.async_receive_from (
	asio::buffer (m_buffer), m_endpoint,
	[this] (const sys::error_code &error, size_t received)
	  { this->handle_receive (error, received); });
  }

  void
  handle_receive (const sys::error_code &error, size_t /*received*/)
  {
    if (!error)
      {
	std::shared_ptr<std::string> message
	    = std::make_shared<std::string> (make_daytime_string ());

	// m_socket.async_send_to (
	//     asio::buffer (*message), m_endpoint,
	//     std::bind (&udp_server::handle_send, this,
	// 	       asio::placeholders::error,
	// 	       asio::placeholders::bytes_transferred, message));
	m_socket.async_send_to (
	    asio::buffer (*message), m_endpoint,
	    [this, message] (const sys::error_code &error, size_t sent)
	      { this->handle_send (error, sent, message); });
      }
    start_receive ();
  }

  void
  handle_send (const sys::error_code & /*error*/, size_t /*sent*/,
	       std::shared_ptr<std::string> /*message*/)
  {
  }
};

int
main ()
{
  try
    {
      asio::io_context ctx;
      udp_server srv (ctx);
      ctx.run ();
    }
  catch (const std::exception &e)
    {
      std::cerr << e.what ();
    }
}
