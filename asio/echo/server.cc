#include <list>

#include <boost/asio.hpp>

namespace sys = boost::system;
namespace asio = boost::asio;
namespace chrono = asio::chrono;
using asio::ip::tcp;

constexpr chrono::seconds default_timeout (5);
constexpr size_t buffer_size = 1024;

class session : public std::enable_shared_from_this<session>
{
public:
  using pointer = std::shared_ptr<session>;

  static pointer
  make (tcp::socket sock,
	chrono::steady_clock::duration timeout = default_timeout)
  {
    return std::make_shared<session> (std::move (sock), timeout);
  }

  explicit session (tcp::socket sock,
		    chrono::steady_clock::duration timeout = default_timeout)
      : socket_ (std::move (sock)), timeout_ (timeout)
  {
  }

  void
  start (bool with_timeout = true)
  {
    if (with_timeout)
      receive_with_timeout ();
    else
      {
	receive_with_watchdog ();
	start_watchdog ();
      }
  }

private:
  void
  stop ()
  {
    socket_.close ();
    timer_.cancel ();
  }

  void
  receive_with_watchdog ()
  {
    auto self = shared_from_this ();
    auto handle_receive = [self] (const sys::error_code &error, size_t bytes)
      {
	if (error)
	  {
	    self->stop ();
	    return;
	  }
	self->send_with_watchdog (bytes);
      };

    socket_.async_receive (asio::buffer (buffer_),
			   asio::bind_executor (strand_, handle_receive));

    deadline_ = chrono::steady_clock::now () + timeout_;
  }

  void
  send_with_watchdog (size_t bytes_to_send)
  {
    auto self = shared_from_this ();
    auto handle_write = [self] (const sys::error_code &error, size_t /*bytes*/)
      {
	if (error)
	  {
	    self->stop ();
	    return;
	  }
	self->receive_with_watchdog ();
      };

    asio::async_write (socket_, asio::buffer (buffer_, bytes_to_send),
		       asio::bind_executor (strand_, handle_write));

    deadline_ = chrono::steady_clock::now () + timeout_;
  }

  void
  start_watchdog ()
  {
    auto now = chrono::steady_clock::now ();
    if (now >= deadline_)
      {
	stop ();
	return;
      }

    auto self = shared_from_this ();
    auto handle_wait = [self] (const sys::error_code &error)
      {
	if (!error)
	  self->start_watchdog ();
      };

    timer_.expires_at (deadline_);
    timer_.async_wait (asio::bind_executor (strand_, handle_wait));
  }

  void
  receive_with_timeout ()
  {
    auto self = shared_from_this ();
    auto handle_receive = [self] (const sys::error_code &error, size_t bytes)
      {
	self->timer_.cancel ();
	if (error)
	  {
	    self->stop ();
	    return;
	  }
	self->send_with_timeout (bytes);
      };

    socket_.async_receive (asio::buffer (buffer_),
			   asio::bind_executor (strand_, handle_receive));

    start_timeout ();
  }

  void
  send_with_timeout (size_t bytes_to_send)
  {
    auto self = shared_from_this ();
    auto handle_write = [self] (const sys::error_code &error, size_t /*bytes*/)
      {
	self->timer_.cancel ();
	if (error)
	  {
	    self->stop ();
	    return;
	  }
	self->receive_with_timeout ();
      };

    asio::async_write (socket_, asio::buffer (buffer_, bytes_to_send),
		       asio::bind_executor (strand_, handle_write));

    start_timeout ();
  }

  void
  start_timeout ()
  {
    auto self = shared_from_this ();
    auto handle_wait = [self] (const sys::error_code &error)
      {
	if (!error)
	  self->stop ();
      };

    timer_.expires_after (timeout_);
    timer_.async_wait (asio::bind_executor (strand_, handle_wait));
  }

private:
  tcp::socket socket_;
  asio::steady_timer timer_{ socket_.get_executor () };
  asio::strand<asio::any_io_executor> strand_{ socket_.get_executor () };

  asio::steady_timer::time_point deadline_;
  chrono::steady_clock::duration timeout_;
  std::array<char, buffer_size> buffer_;
};

class server
{
public:
  server (asio::any_io_executor ex, const tcp::endpoint &ep)
      : acceptor_ (ex, ep)
  {
  }

  server (asio::any_io_executor ex, short port)
      : server (ex, tcp::endpoint (tcp::v4 (), port))
  {
  }

  void
  start ()
  {
    auto handle_accept
	= [this] (const sys::error_code &error, tcp::socket sock)
      {
	if (error)
	  return;

	auto sess = session::make (std::move (sock));
	sess->start (false);

	start ();
      };

    acceptor_.async_accept (handle_accept);
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

      signals.async_wait ([&] (const sys::error_code & /*error*/,
			       int /*signum*/) { io_context.stop (); });

      auto executor = io_context.get_executor ();
      for (int i = 0; i < 10; i++)
	servers.emplace (servers.end (), executor, 8080 + i)->start ();

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
		    std::printf ("exception: %s\n", e.what ());
		  }
	      });

      io_context.run ();
      for (auto &thrd : threads)
	thrd.join ();
    }
  catch (const std::exception &e)
    {
      std::printf ("exception: %s\n", e.what ());
    }
}
