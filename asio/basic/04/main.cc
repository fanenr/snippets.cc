#include <boost/asio.hpp>
#include <functional>
#include <iostream>

namespace asio = boost::asio;

class printer
{
private:
  int m_count = 0;
  asio::steady_timer m_timer;

public:
  printer (asio::io_context &ctx) : m_timer (ctx, asio::chrono::seconds (1))
  {
    // m_timer.async_wait (std::bind (&printer::print, this));
    m_timer.async_wait ([this] () { this->print (); });
  }

  ~printer () { std::cout << "final count: " << m_count << std::endl; }

  void
  print ()
  {
    if (m_count < 5)
      {
	std::cout << m_count++ << std::endl;
	m_timer.expires_at (m_timer.expiry () + asio::chrono::seconds (1));
	// m_timer.async_wait (std::bind (&printer::print, this));
	m_timer.async_wait ([this] () { this->print (); });
      }
  }
};

int
main ()
{
  asio::io_context ctx;
  printer prnt (ctx);
  ctx.run ();
}
