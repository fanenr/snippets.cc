#include <array>
#include <boost/asio.hpp>
#include <iostream>

namespace asio = boost::asio;
using udp = asio::ip::udp;

int
main (int argc, char **argv)
{
  try
    {
      if (argc != 2)
	{
	  std::cerr << "Usage: " << argv[0] << " <host>" << std::endl;
	  return 1;
	}

      asio::io_context ctx;

      udp::resolver resolver (ctx);
      udp::endpoint receiver_endpoint
	  = *resolver.resolve (udp::v4 (), argv[1], "daytime").begin ();

      udp::socket socket (ctx);
      socket.open (udp::v4 ());

      std::array<char, 1> send_buff;
      socket.send_to (asio::buffer (send_buff), receiver_endpoint);

      std::array<char, 128> recv_buff;
      udp::endpoint sender_endpoint;
      size_t size
	  = socket.receive_from (asio::buffer (recv_buff), sender_endpoint);

      std::cout.write (recv_buff.data (), size);
    }
  catch (const std::exception &e)
    {
      std::cerr << e.what ();
    }
}
