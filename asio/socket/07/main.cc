#include <boost/asio.hpp>
#include <ctime>
#include <iostream>
#include <memory>

namespace sys = boost::system;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using udp = asio::ip::udp;

std::string
make_daytime_string ()
{
  time_t now = time (0);
  return ctime (&now);
}

class tcp_connection : public std::enable_shared_from_this<tcp_connection>
{
private:
  tcp::socket m_socket;
  std::string m_message;

public:
  using pointer = std::shared_ptr<tcp_connection>;

  static pointer
  create (asio::io_context &ctx)
  {
    return std::make_shared<tcp_connection> (ctx);
  }

  tcp_connection (asio::io_context &ctx) : m_socket (ctx) {}

  tcp::socket &
  socket ()
  {
    return m_socket;
  }

  void
  start ()
  {
    m_message = make_daytime_string ();

    // asio::async_write (m_socket, asio::buffer (m_message),
    //     	       std::bind (&tcp_connection::handle_write,
    //     			  shared_from_this (),
    //     			  asio::placeholders::error,
    //     			  asio::placeholders::bytes_transferred));
    asio::async_write (m_socket, asio::buffer (m_message),
		       [self = shared_from_this ()] (
			   const sys::error_code &error, size_t wrote)
			 { self->handle_write (error, wrote); });
  }

private:
  void
  handle_write (const sys::error_code & /*error*/, size_t /*wrote*/)
  {
  }
};

class tcp_server
{
private:
  asio::io_context &m_ctx;
  tcp::acceptor m_acceptor;

public:
  tcp_server (asio::io_context &io)
      : m_ctx (io), m_acceptor (io, tcp::endpoint (tcp::v4 (), 13))
  {
    start_accept ();
  }

private:
  void
  start_accept ()
  {
    tcp_connection::pointer conn = tcp_connection::create (m_ctx);

    // m_acceptor.async_accept (conn->socket (),
    //     		     std::bind (&tcp_server::handle_accept, this,
    //     				asio::placeholders::error, conn));
    m_acceptor.async_accept (conn->socket (),
			     [this, conn] (const sys::error_code &error)
			       { this->handle_accept (error, conn); });
  }

  void
  handle_accept (const sys::error_code &error, tcp_connection::pointer conn)
  {
    if (!error)
      conn->start ();
    start_accept ();
  }
};

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
      tcp_server srv1 (ctx);
      udp_server srv2 (ctx);
      ctx.run ();
    }
  catch (const std::exception &e)
    {
      std::cerr << e.what ();
    }
}
