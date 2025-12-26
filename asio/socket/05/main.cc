#include <boost/asio.hpp>
#include <ctime>
#include <iostream>

namespace sys = boost::system;
namespace asio = boost::asio;
using udp = asio::ip::udp;

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

      udp::socket socket (ctx, udp::endpoint (udp::v4 (), 13));

      for (;;)
	{
	  std::array<char, 1> recv_buff;
	  udp::endpoint remote_endpoint;
	  socket.receive_from (asio::buffer (recv_buff), remote_endpoint);

	  std::string message = make_daytime_string ();

	  sys::error_code error;
	  socket.send_to (asio::buffer (message), remote_endpoint, 0, error);
	}
    }
  catch (const std::exception &e)
    {
      std::cerr << e.what ();
    }
}
