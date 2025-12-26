#ifndef SHARED_PTR_H
#define SHARED_PTR_H

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

template <typename T>
class default_delete
{
public:
  default_delete () noexcept = default;

  template <typename U>
  default_delete (const default_delete<U> &)
  {
    static_assert (std::is_convertible<U *, T *>::value,
		   "U* must be convertible to T*");
  }

  void
  operator() (T *ptr) const
  {
    delete ptr;
  }
};

struct control_block_base
{
  std::atomic<long> use_count;
  std::atomic<long> weak_count;

  control_block_base () : use_count (1), weak_count (1) {}

  void
  inc_use_count ()
  {
    use_count.fetch_add (1, std::memory_order_relaxed);
  }

  void
  inc_weak_count ()
  {
    weak_count.fetch_add (1, std::memory_order_relaxed);
  }

  bool
  lock_use_count ()
  {
    long count = use_count.load (std::memory_order_relaxed);
    do
      if (count == 0)
	return false;
    while (!use_count.compare_exchange_weak (count, count + 1,
					     std::memory_order_acq_rel,
					     std::memory_order_relaxed));
    return true;
  }

  void
  dec_use_count ()
  {
    if (use_count.fetch_sub (1, std::memory_order_acq_rel) == 1)
      {
	dispose ();
	dec_weak_count ();
      }
  }

  void
  dec_weak_count ()
  {
    if (weak_count.fetch_sub (1, std::memory_order_acq_rel) == 1)
      destroy ();
  }

  virtual ~control_block_base () = default;
  virtual void dispose () = 0;
  virtual void destroy () = 0;
};

template <typename U, typename Deleter>
struct control_block : public control_block_base
{
  U *ptr;
  Deleter del;

  control_block (U *ptr_, Deleter del_) : ptr (ptr_), del (std::move (del_)) {}

  ~control_block () override = default;

  void
  dispose () override
  {
    del (ptr);
  }

  void
  destroy () override
  {
    delete this;
  }
};

template <typename T>
class weak_ptr;

template <typename T>
class shared_ptr;

template <typename T>
class shared_ptr
{
  template <typename U>
  friend class weak_ptr;

  template <typename U>
  friend class shared_ptr;

public:
  using element_type = T;
  using weak_type = weak_ptr<T>;

  shared_ptr (std::nullptr_t = nullptr) : ptr_ (nullptr), cb_ (nullptr) {}

  template <typename U, typename Deleter = default_delete<U>>
  explicit shared_ptr (U *ptr = nullptr, Deleter del = {})
      : ptr_ (ptr), cb_ (nullptr)
  {
    static_assert (std::is_convertible<U *, T *>::value,
		   "U* must be convertible to T*");
    try
      {
	if (ptr)
	  cb_ = new control_block<U, Deleter> (ptr, std::move (del));
      }
    catch (...)
      {
	del (ptr);
	throw;
      }
  }

  ~shared_ptr ()
  {
    if (cb_)
      cb_->dec_use_count ();
  }

  shared_ptr (const shared_ptr &other) noexcept
      : ptr_ (other.ptr_), cb_ (other.cb_)
  {
    if (cb_)
      cb_->inc_use_count ();
  }

  shared_ptr &
  operator= (const shared_ptr &other) noexcept
  {
    shared_ptr (other).swap (*this);
    return *this;
  }

  template <typename U>
  shared_ptr (const shared_ptr<U> &other) noexcept
      : ptr_ (other.ptr_), cb_ (other.cb_)
  {
    static_assert (std::is_convertible<U *, T *>::value,
		   "U* must be convertible to T*");
    if (cb_)
      cb_->inc_use_count ();
  }

  template <typename U>
  shared_ptr &
  operator= (const shared_ptr<U> &other) noexcept
  {
    shared_ptr (other).swap (*this);
    return *this;
  }

  shared_ptr (shared_ptr &&other) noexcept
      : ptr_ (std::exchange (other.ptr_, nullptr)),
	cb_ (std::exchange (other.cb_, nullptr))
  {
  }

  shared_ptr &
  operator= (shared_ptr &&other) noexcept
  {
    shared_ptr (std::move (other)).swap (*this);
    return *this;
  }

  template <typename U>
  shared_ptr (shared_ptr<U> &&other) noexcept
      : ptr_ (std::exchange (other.ptr_, nullptr)),
	cb_ (std::exchange (other.cb_, nullptr))
  {
    static_assert (std::is_convertible<U *, T *>::value,
		   "U* must be convertible to T*");
  }

  template <typename U>
  shared_ptr &
  operator= (shared_ptr<U> &&other) noexcept
  {
    shared_ptr (std::move (other)).swap (*this);
    return *this;
  }

  void
  swap (shared_ptr &other) noexcept
  {
    using std::swap;
    swap (ptr_, other.ptr_);
    swap (cb_, other.cb_);
  }

  T &
  operator* () const noexcept
  {
    return *ptr_;
  }

  T *
  operator->() const noexcept
  {
    return ptr_;
  }

  explicit
  operator bool () const noexcept
  {
    return ptr_;
  }

  T *
  get () const noexcept
  {
    return ptr_;
  }

  template <typename U = T, typename Deleter = default_delete<U>>
  void
  reset (U *ptr = nullptr, Deleter del = {})
  {
    shared_ptr (ptr, std::move (del)).swap (*this);
  }

  long
  use_count () const noexcept
  {
    return cb_ ? cb_->use_count.load (std::memory_order_relaxed) : 0;
  }

private:
  T *ptr_;
  control_block_base *cb_;
};

#endif // SHARED_PTR_H
