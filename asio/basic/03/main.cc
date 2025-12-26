#include <boost/asio.hpp>
#include <functional>
#include <iostream>

namespace sys = boost::system;
namespace asio = boost::asio;

void
print (const sys::error_code & /*error*/, asio::steady_timer *timer,
       int *count)
{
  if (*count < 5)
    {
      std::cout << (*count)++ << std::endl;
      timer->expires_at (timer->expiry () + asio::chrono::seconds (1));
      // timer->async_wait (
      //     std::bind (print, asio::placeholders::error, timer, count));
      timer->async_wait ([&] (const sys::error_code &error)
			   { print (error, timer, count); });
    }
}

int
main ()
{
  int count = 0;
  asio::io_context ctx;
  asio::steady_timer timer (ctx, asio::chrono::seconds (1));
  // timer.async_wait (
  //     std::bind (print, asio::placeholders::error, &timer, &count));
  timer.async_wait ([&] (const sys::error_code &error)
		      { print (error, &timer, &count); });
  ctx.run ();
  std::cout << "final count is " << count << std::endl;
}
