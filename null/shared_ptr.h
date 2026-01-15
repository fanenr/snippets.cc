#ifndef SHARED_PTR_H
#define SHARED_PTR_H

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

template <bool If>
using enable_if = typename std::enable_if<If, int>::type;

template <class Fn, class... Args>
using enable_if_callable
    = decltype (std::declval<Fn> () (std::declval<Args> ()...), 0);

template <class Fn, class... Args>
struct is_callable
{
private:
  template <class F, class... As>
  static constexpr auto test (...) -> std::false_type;

  template <class F, class... As>
  static constexpr auto test (int)
      -> decltype (std::declval<F> () (std::declval<As> ()...),
		   std::true_type{});

public:
  static constexpr bool value = decltype (test<Fn, Args...> (0))::value;
};

template <class T>
struct default_delete
{
  static_assert (sizeof (T) > 0, "Cannot delete an incomplete type.");

  constexpr default_delete () noexcept = default;

  template <class U, enable_if<std::is_convertible<U *, T *>::value> = 0>
  constexpr default_delete (const default_delete<U> &) noexcept
  {
  }

  void
  operator() (T *p) const
  {
    delete p;
  }
};

struct control_block_base
{
  std::atomic<long> use_count;
  std::atomic<long> weak_count;

  control_block_base () : use_count{ 1 }, weak_count{ 1 } {}
  virtual ~control_block_base () = default;

  void
  inc_use_count ()
  {
    use_count.fetch_add (1, std::memory_order_relaxed);
  }

  void
  dec_use_count ()
  {
    if (use_count.fetch_sub (1, std::memory_order_release) == 1)
      {
	std::atomic_thread_fence (std::memory_order_acquire);
	dispose ();
	dec_weak_count ();
      }
  }

  bool
  lock_use_count ()
  {
    for (long n = use_count.load (std::memory_order_relaxed);;)
      {
	if (n == 0)
	  return false;

	if (use_count.compare_exchange_weak (n, n + 1,
					     std::memory_order_acquire,
					     std::memory_order_relaxed))
	  return true;
      }
  }

  void
  inc_weak_count ()
  {
    weak_count.fetch_add (1, std::memory_order_relaxed);
  }

  void
  dec_weak_count ()
  {
    if (weak_count.fetch_sub (1, std::memory_order_release) == 1)
      {
	std::atomic_thread_fence (std::memory_order_acquire);
	destroy ();
      }
  }

  virtual void dispose () = 0;
  virtual void destroy () = 0;
};

template <class T, class Deleter, enable_if_callable<Deleter, T *> = 0>
struct control_block : control_block_base
{
  T *p;
  Deleter d;

  control_block () = delete;
  control_block (T *p, Deleter d) : p{ p }, d{ std::move (d) } {}

  void
  dispose () override
  {
    d (p);
  }

  void
  destroy () override
  {
    delete this;
  }
};

template <class T>
class weak_ptr;

template <class T>
class shared_ptr;

template <class T>
class shared_ptr
{
  template <class>
  friend class weak_ptr;

  template <class>
  friend class shared_ptr;

public:
  typedef T *pointer;
  typedef T element_type;
  typedef weak_ptr<T> weak_type;

  shared_ptr () noexcept : px_{}, pb_{} {}
  shared_ptr (std::nullptr_t) noexcept : shared_ptr{} {}

  template <class U, class Deleter = default_delete<U>,
	    enable_if<std::is_convertible<U *, T *>::value
		      && is_callable<Deleter, U *>::value>
	    = 0>
  explicit shared_ptr (U *p = nullptr, Deleter d = {}) : px_{ p }, pb_{}
  {
    try
      {
	pb_ = new control_block<U, Deleter>{ p, std::move (d) };
      }
    catch (...)
      {
	d (p);
	throw;
      }
  }

  ~shared_ptr ()
  {
    if (pb_)
      pb_->dec_use_count ();
  }

  shared_ptr (const shared_ptr &other) noexcept
      : px_{ other.px_ }, pb_{ other.pb_ }
  {
    if (pb_)
      pb_->inc_use_count ();
  }

  shared_ptr &
  operator= (const shared_ptr &other) noexcept
  {
    shared_ptr t{ other };
    swap (*this, t);
    return *this;
  }

  template <class U, enable_if<std::is_convertible<U *, T *>::value> = 0>
  shared_ptr (const shared_ptr<U> &other) noexcept
      : px_{ other.px_ }, pb_{ other.pb_ }
  {
    if (pb_)
      pb_->inc_use_count ();
  }

  template <class U, enable_if<std::is_convertible<U *, T *>::value> = 0>
  shared_ptr &
  operator= (const shared_ptr<U> &other) noexcept
  {
    shared_ptr t{ other };
    swap (*this, t);
    return *this;
  }

  shared_ptr (shared_ptr &&other) noexcept : px_{ other.px_ }, pb_{ other.pb_ }
  {
    other.px_ = nullptr;
    other.pb_ = nullptr;
  }

  shared_ptr &
  operator= (shared_ptr &&other) noexcept
  {
    shared_ptr t{ std::move (other) };
    swap (*this, t);
    return *this;
  }

  template <class U, enable_if<std::is_convertible<U *, T *>::value> = 0>
  shared_ptr (shared_ptr<U> &&other) noexcept
      : px_{ other.px_ }, pb_{ other.pb_ }
  {
    other.px_ = nullptr;
    other.pb_ = nullptr;
  }

  template <class U, enable_if<std::is_convertible<U *, T *>::value> = 0>
  shared_ptr &
  operator= (shared_ptr<U> &&other) noexcept
  {
    shared_ptr t{ std::move (other) };
    swap (*this, t);
    return *this;
  }

  T &
  operator* () const noexcept (noexcept (*get ()))
  {
    return *px_;
  }

  T *
  operator->() const noexcept
  {
    return px_;
  }

  explicit
  operator bool () const noexcept
  {
    return px_;
  }

  T *
  get () const noexcept
  {
    return px_;
  }

  template <class U = T, class E = default_delete<U>,
	    enable_if<std::is_convertible<U *, T *>::value
		      && is_callable<E, U *>::value>
	    = 0>
  void
  reset (U *p = nullptr, E d = {})
  {
    shared_ptr t{ p, std::move (d) };
    swap (*this, t);
  }

  long
  use_count () const noexcept
  {
    return pb_ ? pb_->use_count.load (std::memory_order_relaxed) : 0;
  }

  friend void
  swap (shared_ptr &a, shared_ptr &b) noexcept
  {
    using std::swap;
    swap (a.px_, b.px_);
    swap (a.pb_, b.pb_);
  }

private:
  pointer px_;
  control_block_base *pb_;
};

#endif // SHARED_PTR_H
