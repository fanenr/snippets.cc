#include <boost/asio.hpp>
#include <functional>
#include <iostream>
#include <thread>

namespace asio = boost::asio;

class printer
{
private:
  int m_count = 0;
  asio::steady_timer m_timer1;
  asio::steady_timer m_timer2;
  asio::strand<asio::io_context::executor_type> m_strand;

public:
  printer (asio::io_context &ctx)
      : m_timer1 (ctx, asio::chrono::seconds (1)),
	m_timer2 (ctx, asio::chrono::seconds (1)),
	m_strand (asio::make_strand (ctx))
  {
    // m_timer1.async_wait (
    //     asio::bind_executor (m_strand, std::bind (&printer::print1, this)));
    m_timer1.async_wait (
	asio::bind_executor (m_strand, [this] () { this->print1 (); }));

    // m_timer2.async_wait (
    //     asio::bind_executor (m_strand, std::bind (&printer::print2, this)));
    m_timer2.async_wait (
	asio::bind_executor (m_strand, [this] () { this->print2 (); }));
  }

  ~printer () { std::cout << "final count is " << m_count << std::endl; }

  void
  print1 ()
  {
    if (m_count < 10)
      {
	std::cout << "timer1: " << m_count++ << std::endl;

	m_timer1.expires_at (m_timer1.expiry () + asio::chrono::seconds (1));

	// m_timer1.async_wait (asio::bind_executor (
	//     m_strand, std::bind (&printer::print1, this)));
	m_timer1.async_wait (
	    asio::bind_executor (m_strand, [this] () { this->print1 (); }));
      }
  }

  void
  print2 ()
  {
    if (m_count < 10)
      {
	std::cout << "timer2: " << m_count++ << std::endl;

	m_timer2.expires_at (m_timer2.expiry () + asio::chrono::seconds (1));

	// m_timer2.async_wait (asio::bind_executor (
	//     m_strand, std::bind (&printer::print2, this)));
	m_timer2.async_wait (
	    asio::bind_executor (m_strand, [this] () { this->print2 (); }));
      }
  }
};

int
main ()
{
  asio::io_context ctx;
  printer prnt (ctx);
  std::thread thrd ([&] { ctx.run (); });
  ctx.run ();
  thrd.join ();
}
