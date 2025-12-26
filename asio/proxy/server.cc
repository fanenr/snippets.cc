#include <boost/asio.hpp>

namespace sys = boost::system;
namespace asio = boost::asio;
using asio::ip::tcp;

constexpr asio::chrono::seconds default_timeout (5);
constexpr size_t buffer_size = 1024;

class watchdog
{
public:
  explicit watchdog (asio::any_io_executor ex) : timer_ (ex) {}

  template <typename Callback>
  void
  start (Callback cb)
  {
    auto now = asio::chrono::steady_clock::now ();
    if (now >= deadline_)
      {
	cb ();
	return;
      }

    auto handle_wait = [this, cb] (const sys::error_code &error)
      {
	if (!error)
	  start (cb);
      };

    timer_.expires_at (deadline_);
    timer_.async_wait (handle_wait);
  }

  void
  stop ()
  {
    timer_.cancel ();
  }

  void
  delay (asio::chrono::steady_clock::duration dur)
  {
    deadline_ = asio::chrono::steady_clock::now () + dur;
  }

private:
  asio::steady_timer timer_;
  asio::steady_timer::time_point deadline_;
};

class session : public std::enable_shared_from_this<session>
{
public:
  using pointer = std::shared_ptr<session>;

  static pointer
  make (tcp::socket client,
	asio::chrono::steady_clock::duration timeout = default_timeout)
  {
    return std::make_shared<session> (std::move (client), timeout);
  }

  explicit session (tcp::socket client,
		    asio::chrono::steady_clock::duration timeout
		    = default_timeout)
      : client_ (std::move (client)), timeout_ (timeout)
  {
  }

  void
  start (const tcp::endpoint &target)
  {
    auto self = shared_from_this ();
    auto handle_connect = [self] (const sys::error_code &error)
      {
	if (error)
	  return;
	self->receive_from_client ();
	self->receive_from_server ();
	self->start_watchdogs ();
      };

    server_.async_connect (target, handle_connect);
  }

private:
  void
  stop ()
  {
    client_.close ();
    server_.close ();
    watchdog1_.stop ();
    watchdog2_.stop ();
  }

  void
  receive_from_client ()
  {
    auto self = shared_from_this ();
    auto handle_receive = [self] (const sys::error_code &error, size_t bytes)
      {
	if (error)
	  {
	    self->stop ();
	    return;
	  }
	self->send_to_server (bytes);
      };

    client_.async_receive (asio::buffer (client_buffer_),
			   asio::bind_executor (strand1_, handle_receive));
    watchdog1_.delay (timeout_);
  }

  void
  send_to_server (size_t bytes_to_send)
  {
    auto self = shared_from_this ();
    auto handle_write = [self] (const sys::error_code &error, size_t /*bytes*/)
      {
	if (error)
	  {
	    self->stop ();
	    return;
	  }
	self->receive_from_client ();
      };

    asio::async_write (server_, asio::buffer (client_buffer_, bytes_to_send),
		       asio::bind_executor (strand1_, handle_write));
    watchdog1_.delay (timeout_);
  }

  void
  receive_from_server ()
  {
    auto self = shared_from_this ();
    auto handle_receive = [self] (const sys::error_code &error, size_t bytes)
      {
	if (error)
	  {
	    self->stop ();
	    return;
	  }
	self->send_to_client (bytes);
      };

    server_.async_receive (asio::buffer (server_buffer_),
			   asio::bind_executor (strand2_, handle_receive));
    watchdog2_.delay (timeout_);
  }

  void
  send_to_client (size_t bytes_to_send)
  {
    auto self = shared_from_this ();
    auto handle_write = [self] (const sys::error_code &error, size_t /*bytes*/)
      {
	if (error)
	  {
	    self->stop ();
	    return;
	  }
	self->receive_from_server ();
      };

    asio::async_write (client_, asio::buffer (server_buffer_, bytes_to_send),
		       asio::bind_executor (strand2_, handle_write));
    watchdog2_.delay (timeout_);
  }

  void
  start_watchdogs ()
  {
    auto self = shared_from_this ();
    auto callback{ [self] () { self->stop (); } };

    watchdog1_.start (callback);
    watchdog2_.start (callback);
  }

private:
  tcp::socket client_;
  tcp::socket server_{ client_.get_executor () };
  asio::strand<asio::any_io_executor> strand1_{ client_.get_executor () };
  asio::strand<asio::any_io_executor> strand2_{ client_.get_executor () };

  watchdog watchdog1_{ strand1_ };
  watchdog watchdog2_{ strand2_ };
  asio::chrono::steady_clock::duration timeout_;

  std::array<char, buffer_size> client_buffer_;
  std::array<char, buffer_size> server_buffer_;
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
  start (const tcp::endpoint &target)
  {
    target_ = target;
    start ();
  }

private:
  void
  start ()
  {
    auto handle_accept
	= [this] (const sys::error_code &error, tcp::socket sock)
      {
	if (error)
	  return;

	auto sess = session::make (std::move (sock));
	sess->start (target_);

	start ();
      };

    acceptor_.async_accept (handle_accept);
  }

private:
  tcp::acceptor acceptor_;
  tcp::endpoint target_;
};

int
main (int argc, char **argv)
{
  try
    {
      if (argc != 5)
	{
	  std::printf ("Usage: %s "
		       "<listen_address> <listen_port> "
		       "<target_address> <target_port>\n",
		       argv[0]);
	  return 1;
	}

      asio::io_context io_context;
      tcp::resolver resolver (io_context);

      auto listen_endpoint
	  = resolver.resolve (argv[1], argv[2]).begin ()->endpoint ();
      auto target_endpoint
	  = resolver.resolve (argv[3], argv[4]).begin ()->endpoint ();

      server srv (io_context.get_executor (), listen_endpoint);
      srv.start (target_endpoint);
      io_context.run ();
    }
  catch (const std::exception &e)
    {
      std::printf ("exception: %s\n", e.what ());
    }
}
