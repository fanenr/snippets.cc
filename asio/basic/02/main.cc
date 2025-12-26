#include <boost/asio.hpp>
#include <iostream>

namespace sys = boost::system;
namespace asio = boost::asio;

void
print (const sys::error_code & /*error*/)
{
  std::cout << "Hello, world!" << std::endl;
}

int
main ()
{
  asio::io_context ctx;
  asio::steady_timer timer (ctx, asio::chrono::seconds (5));
  timer.async_wait (print);
  ctx.run ();
  std::cout << "exit." << std::endl;
}
