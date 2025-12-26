#include <ctime>
#include <iostream>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

namespace sys = boost::system;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

std::string
make_daytime_string ()
{
  time_t now = time (0);
  return ctime (&now);
}

class tcp_connection : public std::enable_shared_from_this<tcp_connection>
{
public:
  using pointer = std::shared_ptr<tcp_connection>;

  static pointer
  make (asio::io_context &ctx)
  {
    return std::make_shared<tcp_connection> (ctx);
  }

  explicit tcp_connection (asio::io_context &io) : socket_ (io) {}

  void
  start (asio::any_io_executor ex)
  {
    auto go = [this] (asio::yield_context yield)
      {
	sys::error_code ec;
	auto msg = make_daytime_string ();
	asio::async_write (socket_, asio::buffer (msg), yield[ec]);
      };

    asio::spawn (ex, go, asio::detached);
  }

private:
  tcp::socket socket_;
  friend class tcp_server;
};

class tcp_server
{
public:
  tcp_server (asio::io_context &io)
      : io_context_ (io), acceptor_ (io, tcp::endpoint (tcp::v4 (), 13))
  {
    auto go = [this] (asio::yield_context yield)
      {
	for (;;)
	  {
	    auto conn = tcp_connection::make (io_context_);
	    acceptor_.async_accept (conn->socket_, yield);
	    conn->start (yield.get_executor ());
	  }
      };

    asio::spawn (io_context_, go, asio::detached);
  }

private:
  asio::io_context &io_context_;
  tcp::acceptor acceptor_;
};

int
main ()
{
  try
    {
      asio::io_context ctx;
      tcp_server srv (ctx);
      ctx.run ();
    }
  catch (const std::exception &e)
    {
      std::cerr << e.what ();
    }
}
