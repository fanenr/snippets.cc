#ifndef WEAK_PTR_H
#define WEAK_PTR_H

#include "shared_ptr.h"

template <class T>
class weak_ptr
{
  template <class>
  friend class weak_ptr;

  template <class>
  friend class shared_ptr;

public:
  typedef T *pointer;
  typedef T element_type;

  constexpr weak_ptr () : px_{}, pb_{} {}

  ~weak_ptr ()
  {
    if (pb_)
      pb_->dec_weak_count ();
  }

  template <class U, enable_if<std::is_convertible<U *, T *>::value> = 0>
  weak_ptr (const shared_ptr<U> &sp) noexcept : px_{ sp.px_ }, pb_{ sp.pb_ }
  {
    if (pb_)
      pb_->inc_weak_count ();
  }

  template <class U, enable_if<std::is_convertible<U *, T *>::value> = 0>
  weak_ptr &
  operator= (const shared_ptr<U> &sp) noexcept
  {
    weak_ptr t{ sp };
    swap (*this, t);
    return *this;
  }

  weak_ptr (const weak_ptr &other) noexcept
      : px_{ other.px_ }, pb_{ other.pb_ }
  {
    if (pb_)
      pb_->inc_weak_count ();
  }

  weak_ptr &
  operator= (const weak_ptr &other) noexcept
  {
    weak_ptr t{ other };
    swap (*this, t);
    return *this;
  }

  template <class U, enable_if<std::is_convertible<U *, T *>::value> = 0>
  weak_ptr (const weak_ptr<U> &other) noexcept
      : px_{ other.px_ }, pb_{ other.pb_ }
  {
    if (pb_)
      pb_->inc_weak_count ();
  }

  template <class U, enable_if<std::is_convertible<U *, T *>::value> = 0>
  weak_ptr &
  operator= (const weak_ptr<U> &other) noexcept
  {
    weak_ptr t{ other };
    swap (*this, t);
    return *this;
  }

  weak_ptr (weak_ptr &&other) noexcept : px_{ other.px_ }, pb_{ other.pb_ }
  {
    other.px_ = nullptr;
    other.pb_ = nullptr;
  }

  weak_ptr &
  operator= (weak_ptr &&other) noexcept
  {
    weak_ptr t{ std::move (other) };
    swap (*this, t);
    return *this;
  }

  template <class U, enable_if<std::is_convertible<U *, T *>::value> = 0>
  weak_ptr (weak_ptr<U> &&other) noexcept : px_{ other.px_ }, pb_{ other.pb_ }
  {
    other.px_ = nullptr;
    other.pb_ = nullptr;
  }

  template <class U, enable_if<std::is_convertible<U *, T *>::value> = 0>
  weak_ptr &
  operator= (weak_ptr<U> &&other) noexcept
  {
    weak_ptr t{ std::move (other) };
    swap (*this, t);
    return *this;
  }

  shared_ptr<T>
  lock () noexcept
  {
    if (pb_ && pb_->lock_use_count ())
      {
	shared_ptr<T> sp;
	sp.px_ = px_;
	sp.pb_ = pb_;
	return sp;
      }
    return {};
  }

  void
  reset () noexcept
  {
    weak_ptr t;
    swap (*this, t);
  }

  bool
  expired () const noexcept
  {
    return use_count () == 0;
  }

  long
  use_count () const noexcept
  {
    return pb_ ? pb_->use_count.load (std::memory_order_relaxed) : 0;
  }

  friend void
  swap (weak_ptr &a, weak_ptr &b) noexcept
  {
    using std::swap;
    swap (a.px_, b.px_);
    swap (a.pb_, b.pb_);
  }

private:
  pointer px_;
  control_block_base *pb_;
};

#endif // WEAK_PTR_H
