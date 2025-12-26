#include <list>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/context/fixedsize_stack.hpp>
#include <boost/context/pooled_fixedsize_stack.hpp>

namespace sys = boost::system;
namespace asio = boost::asio;
using asio::ip::tcp;

constexpr asio::chrono::seconds default_timeout (5);
constexpr size_t max_receive = 1024;

using allocator_t = boost::context::fixedsize_stack;
allocator_t allocator (8 * 1024);

void
handle_spawn (std::exception_ptr e)
{
  if (e)
    std::rethrow_exception (e);
}

class session : public std::enable_shared_from_this<session>
{
public:
  using pointer = std::shared_ptr<session>;

  static pointer
  make (tcp::socket sock, asio::chrono::seconds timeout = default_timeout)
  {
    return std::make_shared<session> (std::move (sock), timeout);
  }

  explicit session (tcp::socket sock,
		    asio::chrono::seconds timeout = default_timeout)
      : socket_ (std::move (sock)), timeout_ (timeout)
  {
  }

  void
  start (bool with_timeout = true)
  {
    if (with_timeout)
      start_with_timeout ();
    else
      start_with_watchdog ();
  }

private:
  void
  start_with_timeout ()
  {
    auto self = shared_from_this ();
    auto echo = [this, self] (asio::yield_context yield)
      {
	for (;;)
	  {
	    size_t n;
	    sys::error_code ec;

	    start_timeout ();
	    buffer_.resize (max_receive);
	    n = socket_.async_receive (asio::buffer (buffer_), yield[ec]);
	    timer_.cancel ();
	    if (ec)
	      break;

	    start_timeout ();
	    buffer_.resize (n);
	    asio::async_write (socket_, asio::buffer (buffer_), yield[ec]);
	    timer_.cancel ();
	    if (ec)
	      break;
	  }
      };
    asio::spawn (strand_, std::allocator_arg, allocator, echo, handle_spawn);
  }

  void
  start_timeout ()
  {
    auto self = shared_from_this ();
    auto wait = [this, self] (asio::yield_context yield)
      {
	timer_.expires_after (timeout_);

	sys::error_code ec;
	timer_.async_wait (yield[ec]);

	if (!ec)
	  socket_.close ();
      };
    asio::spawn (strand_, std::allocator_arg, allocator, wait, handle_spawn);
  }

  void
  start_with_watchdog ()
  {
    auto self = shared_from_this ();
    auto echo = [this, self] (asio::yield_context yield)
      {
	for (;;)
	  {
	    size_t n;
	    sys::error_code ec;

	    deadline_ = asio::chrono::steady_clock::now () + timeout_;
	    buffer_.resize (max_receive);
	    n = socket_.async_receive (asio::buffer (buffer_), yield[ec]);
	    if (ec)
	      {
		timer_.cancel ();
		break;
	      }

	    deadline_ = asio::chrono::steady_clock::now () + timeout_;
	    buffer_.resize (n);
	    asio::async_write (socket_, asio::buffer (buffer_), yield[ec]);
	    if (ec)
	      {
		timer_.cancel ();
		break;
	      }
	  }
      };
    asio::spawn (strand_, std::allocator_arg, allocator, echo, handle_spawn);

    start_watchdog ();
  }

  void
  start_watchdog ()
  {
    auto self = shared_from_this ();
    auto watch = [this, self] (asio::yield_context yield)
      {
	for (;;)
	  {
	    auto now = asio::chrono::steady_clock::now ();
	    if (now >= deadline_)
	      {
		socket_.close ();
		break;
	      }

	    sys::error_code ec;
	    timer_.expires_at (deadline_);
	    timer_.async_wait (yield[ec]);
	    if (ec)
	      break;
	  }
      };
    asio::spawn (strand_, std::allocator_arg, allocator, watch, handle_spawn);
  }

private:
  tcp::socket socket_;
  asio::steady_timer timer_{ socket_.get_executor () };
  asio::strand<asio::any_io_executor> strand_{ socket_.get_executor () };

  std::vector<char> buffer_;
  asio::chrono::seconds timeout_;
  asio::steady_timer::time_point deadline_;
};

class server
{
public:
  explicit server (asio::io_context &io, short port)
      : acceptor_ (io, tcp::endpoint (tcp::v4 (), port))
  {
  }

  void
  start ()
  {
    auto listen = [this] (asio::yield_context yield)
      {
	for (;;)
	  {
	    auto sock = acceptor_.async_accept (yield);
	    auto sess = session::make (std::move (sock));
	    sess->start (false);
	  }
      };

    asio::spawn (acceptor_.get_executor (), std::allocator_arg, allocator,
		 listen, handle_spawn);
  }

private:
  tcp::acceptor acceptor_;
};

int
main ()
{
  try
    {
      asio::io_context io_context;
      asio::signal_set signals (io_context, SIGINT, SIGTERM);
      std::list<server> servers;
      std::vector<std::thread> threads;

      auto wait = [&signals, &io_context] (asio::yield_context yield)
	{
	  signals.async_wait (yield);
	  io_context.stop ();
	};

      asio::spawn (io_context, std::allocator_arg, allocator, wait,
		   handle_spawn);

      for (int i = 0; i < 10; i++)
	servers.emplace (servers.end (), io_context, 8080 + i)->start ();

      unsigned int num_threads = std::thread::hardware_concurrency ();
      num_threads = num_threads ? num_threads * 2 : 10;
      for (unsigned int i = 0; i < num_threads; i++)
	threads.emplace_back (
	    [&io_context] ()
	      {
		try
		  {
		    io_context.run ();
		  }
		catch (const std::exception &e)
		  {
		    printf ("exception: %s\n", e.what ());
		  }
	      });

      io_context.run ();
      for (auto &thrd : threads)
	thrd.join ();
    }
  catch (const std::exception &e)
    {
      printf ("exception: %s\n", e.what ());
    }
}
