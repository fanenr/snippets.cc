#ifndef UNIQUE_PTR_H
#define UNIQUE_PTR_H

#include <cstddef>
#include <type_traits>
#include <utility>

template <class T>
class default_delete
{
public:
  static_assert (sizeof (T) > 0, "Cannot delete an incomplete type.");

  constexpr default_delete () noexcept = default;

  template <class U,
	    typename std::enable_if<std::is_convertible<U *, T *>::value>::type
		* = nullptr>
  constexpr default_delete (const default_delete<U> &) noexcept
  {
  }

  void
  operator() (T *p) const
  {
    delete p;
  }
};

template <class T, class Deleter>
class delete_wrapper
{
  template <class, class>
  friend class delete_wrapper;

public:
  template <
      decltype (std::declval<Deleter> () (static_cast<T *> (0))) * = nullptr>
  constexpr delete_wrapper (Deleter del) : d_{ del }
  {
  }

  template <class Y, class D,
	    typename std::enable_if<
		std::is_convertible<Y *, T *>::value
		&& std::is_convertible<D, Deleter>::value>::type * = nullptr>
  constexpr delete_wrapper (const delete_wrapper<Y, D> &other) noexcept
      : d_{ other.d_ }
  {
  }

  operator Deleter &() { return d_; }
  operator const Deleter &() const { return d_; }

  void
  operator() (T *ptr)
  {
    d_ (ptr);
  }

private:
  Deleter d_;
};

template <
    class T, class D,
    class Deleter = typename std::conditional<std::is_class<D>::value, D,
					      delete_wrapper<T, D>>::type,
    decltype (std::declval<Deleter> () (static_cast<T *> (0))) * = nullptr>
using unique_ptr_base = Deleter;

template <class T, class Deleter = default_delete<T>>
class unique_ptr : private unique_ptr_base<T, Deleter>
{
  template <class, class>
  friend class unique_ptr;

  typedef unique_ptr_base<T, Deleter> base_type;

public:
  typedef T *pointer;
  typedef T element_type;
  typedef Deleter deleter_type;

  unique_ptr () noexcept : base_type{}, p_{} {}
  unique_ptr (std::nullptr_t) noexcept : unique_ptr{} {}

  explicit unique_ptr (pointer p) : base_type{}, p_{ p } {}
  unique_ptr (pointer p, deleter_type d) : base_type{ std::move (d) }, p_{ p }
  {
  }

  ~unique_ptr ()
  {
    if (p_)
      (*this) (p_);
  }

  unique_ptr (const unique_ptr &) = delete;
  unique_ptr &operator= (const unique_ptr &) = delete;

  unique_ptr (unique_ptr &&other) noexcept
      : base_type{ std::move (other) }, p_ (other.p_)
  {
    other.p_ = nullptr;
  }

  unique_ptr &
  operator= (unique_ptr &&other) noexcept
  {
    return assign (std::move (other));
  }

  template <class U, class E,
	    typename std::enable_if<
		std::is_convertible<U *, T *>::value
		&& std::is_convertible<E, Deleter>::value>::type * = nullptr>
  unique_ptr (unique_ptr<U, E> &&other) noexcept
      : base_type{ std::move (other) }, p_{ other.p_ }

  {
    other.p_ = nullptr;
  }

  template <class U, class E,
	    typename std::enable_if<
		std::is_convertible<U *, T *>::value
		&& std::is_convertible<E, Deleter>::value>::type * = nullptr>
  unique_ptr &
  operator= (unique_ptr<U, E> &&other) noexcept
  {
    return assign (std::move (other));
  }

  unique_ptr &
  operator= (std::nullptr_t) noexcept
  {
    return assign (nullptr);
  }

  friend void
  swap (unique_ptr &a, unique_ptr &b) noexcept
  {
    using std::swap;
    swap (static_cast<base_type &> (a), static_cast<base_type &> (b));
    swap (a.p_, b.p_);
  }

  element_type &
  operator* () const noexcept (noexcept (*get ()))
  {
    return *p_;
  }

  pointer
  operator->() const noexcept
  {
    return p_;
  }

  explicit
  operator bool () const noexcept
  {
    return p_;
  }

  pointer
  get () const noexcept
  {
    return p_;
  }

  void
  reset (pointer p = nullptr) noexcept
  {
    assign (unique_ptr{ p });
  }

  pointer
  release () noexcept
  {
    pointer p = p_;
    p_ = nullptr;
    return p;
  }

  deleter_type &
  get_deleter () noexcept
  {
    return static_cast<deleter_type &> (*this);
  }

  const deleter_type &
  get_deleter () const noexcept
  {
    return static_cast<const deleter_type &> (*this);
  }

private:
  unique_ptr &
  assign (unique_ptr t) noexcept
  {
    using std::swap;
    swap (*this, t);
    return *this;
  }

private:
  pointer p_;
};

#endif // UNIQUE_PTR_H
