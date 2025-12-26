#include <thread>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/context/fixedsize_stack.hpp>
#include <boost/context/pooled_fixedsize_stack.hpp>

namespace sys = boost::system;
namespace asio = boost::asio;
using asio::ip::tcp;

using allocator_t = boost::context::fixedsize_stack;
allocator_t allocator (8 * 1024);

int connections;
std::atomic<bool> stop;
std::atomic<int> echoes;
std::atomic<int> failed;
std::atomic<int> connected;
std::atomic<int> completed;

void monitor ();

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
  make (asio::io_context &io)
  {
    return std::make_shared<session> (io);
  }

  explicit session (asio::io_context &io)
      : socket_ (io, tcp::v4 ()), timer_ (io)
  {
  }

  void
  start (short port, asio::yield_context yield)
  {
    sys::error_code ec;

    // connect
    socket_.async_connect (tcp::endpoint (tcp::v4 (), port), yield[ec]);
    completed++;
    if (ec)
      {
	failed++;
	return;
      }
    connected++;

    // echo
    for (;;)
      {
	// send
	send_buffer_ = "Hello world!";
	recv_buffer_.resize (send_buffer_.size ());
	asio::async_write (socket_, asio::buffer (send_buffer_), yield[ec]);
	if (ec)
	  {
	    connected--;
	    failed++;
	    break;
	  }

	// recv
	asio::async_read (socket_, asio::buffer (recv_buffer_), yield[ec]);
	if (ec)
	  {
	    connected--;
	    failed++;
	    break;
	  }

	// check
	if (recv_buffer_ == send_buffer_)
	  echoes++;

	// delay
	timer_.expires_after (asio::chrono::seconds (1));
	timer_.async_wait (yield[ec]);
      }
  }

private:
  tcp::socket socket_;
  std::string send_buffer_;
  std::string recv_buffer_;
  asio::steady_timer timer_;
};

int
main (int argc, char **argv)
{
  if (argc != 2)
    {
      printf ("Usage: %s <connections>\n", argv[0]);
      return 1;
    }

  signal (SIGPIPE, SIG_IGN);
  signal (SIGINT,
	  [] (int signum)
	    {
	      if (signum == SIGINT)
		{
		  stop = true;
		  puts ("");
		}
	    });

  connections = std::atoi (argv[1]);
  std::vector<std::thread> echo_thrds;
  std::vector<asio::io_context> ctxs (10);

  for (int i = 0; i < 10; i++)
    {
      auto &io = ctxs[i];
      short port = 8080 + i;
      echo_thrds.emplace_back (
	  [&io, port] ()
	    {
	      try
		{
		  int conns = connections / 10;
		  conns += !!(connections % 10);

		  for (int j = 0; j < conns; j++)
		    {
		      auto sess = session::make (io);
		      auto go = [sess, port] (asio::yield_context yield)
			{ sess->start (port, yield); };
		      asio::spawn (io, std::allocator_arg, allocator, go,
				   handle_spawn);
		    }

		  io.run ();
		}
	      catch (const std::exception &e)
		{
		  printf ("Exception: %s\n", e.what ());
		}
	    });
    }

  std::thread monitor_thrd (monitor);

  for (; !stop;)
    std::this_thread::sleep_for (std::chrono::milliseconds (100));

  for (auto &io : ctxs)
    io.stop ();
  for (auto &thrd : echo_thrds)
    thrd.join ();
  monitor_thrd.join ();
}

void
monitor ()
{
  auto print_line = [] ()
    {
      for (int i = 0; i < 75; i++)
	putchar ('-');
      puts ("");
    };

  printf ("Target: 127.0.0.1:8080-8089 | Total Connections: %d\n",
	  connections);
  print_line ();

  auto start = std::chrono::steady_clock::now ();
  for (; !stop && completed < connections;
       std::this_thread::sleep_for (std::chrono::milliseconds (100)))
    {
      printf ("\rConnecting: %d / %d", connected.load (), connections);
      fflush (stdout);
    }
  auto end = std::chrono::steady_clock::now ();

  double seconds = std::chrono::duration<double> (end - start).count ();
  printf ("\rEstablished %d connections in %.2f seconds (%d failed).\n",
	  connected.load (), seconds, failed.load ());

  if (stop)
    return;
  print_line ();

  const char *fmt = "Active: %6d | "
		    "Failed: %6d | "
		    "Echoes: %9d | "
		    "Rate: %8d echo/s\n";

  int last = 0, times = 0;
  for (; !stop; times++)
    {
      if (connected.load () == 0)
	{
	  stop = true;
	  break;
	}

      int now = echoes.load ();
      int rate = now - last;
      last = now;

      printf (fmt, connected.load (), failed.load (), now, rate);
      std::this_thread::sleep_for (std::chrono::seconds (1));
    }

  print_line ();
  int total = echoes.load ();
  int rate = (times == 0 ? 0 : total / times);
  printf (fmt, connected.load (), failed.load (), total, rate);
}
