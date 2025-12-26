#include <array>
#include <boost/asio.hpp>
#include <iostream>

namespace sys = boost::system;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

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

      tcp::resolver resolver (ctx);
      tcp::resolver::results_type endpoints
	  = resolver.resolve (argv[1], "daytime");

      tcp::socket socket (ctx);
      asio::connect (socket, endpoints);

      for (;;)
	{
	  std::array<char, 128> buff;

	  sys::error_code error;
	  size_t len = socket.read_some (asio::buffer (buff), error);

	  if (error == asio::error::eof)
	    break;
	  else if (error)
	    throw sys::system_error (error);

	  std::cout.write (buff.data (), len);
	}
    }
  catch (const std::exception &e)
    {
      std::cerr << e.what () << std::endl;
    }
}
