#include <boost/asio.hpp>
#include <ctime>
#include <iostream>
#include <string>

namespace sys = boost::system;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

std::string
make_daytime_string ()
{
  time_t now = time (0);
  return ctime (&now);
}

int
main ()
{
  try
    {
      asio::io_context ctx;

      tcp::acceptor acceptor (ctx, tcp::endpoint (tcp::v4 (), 13));

      for (;;)
	{
	  tcp::socket socket (ctx);
	  acceptor.accept (socket);

	  std::string message = make_daytime_string ();

	  sys::error_code error;
	  asio::write (socket, asio::buffer (message), error);
	}
    }
  catch (const std::exception &e)
    {
      std::cerr << e.what ();
    }
}
