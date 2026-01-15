#ifndef UNIQUE_PTR_H
#define UNIQUE_PTR_H

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

template <class T, class Deleter, enable_if_callable<Deleter, T *> = 0>
struct delete_wrapper
{
  Deleter d;

  constexpr delete_wrapper () noexcept = delete;
  constexpr delete_wrapper (Deleter d) : d{ d } {}

  template <class U, class E,
	    enable_if<std::is_convertible<U *, T *>::value
		      && std::is_convertible<E, Deleter>::value>
	    = 0>
  constexpr delete_wrapper (const delete_wrapper<U, E> &other) noexcept
      : d{ other.d }
  {
  }

  operator Deleter &() { return d; }
  operator const Deleter &() const { return d; }

  void
  operator() (T *ptr) const
  {
    d (ptr);
  }
};

template <class T, class D,
	  class Deleter = typename std::conditional<
	      std::is_class<D>::value, D, delete_wrapper<T, D>>::type,
	  enable_if_callable<Deleter, T *> = 0>
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

  unique_ptr (unique_ptr &&other) noexcept : unique_ptr{}
  {
    swap (*this, other);
  }

  unique_ptr &
  operator= (unique_ptr &&other) noexcept
  {
    unique_ptr t{ std::move (other) };
    swap (*this, t);
    return *this;
  }

  template <class U, class E,
	    enable_if<std::is_convertible<U *, T *>::value
		      && std::is_convertible<E, Deleter>::value>
	    = 0>
  unique_ptr (unique_ptr<U, E> &&other) noexcept
      : base_type{ std::move (other.get_deleter ()) }, p_{ other.release () }
  {
  }

  template <class U, class E,
	    enable_if<std::is_convertible<U *, T *>::value
		      && std::is_convertible<E, Deleter>::value>
	    = 0>
  unique_ptr &
  operator= (unique_ptr<U, E> &&other) noexcept
  {
    unique_ptr t{ std::move (other) };
    swap (*this, t);
    return *this;
  }

  unique_ptr &
  operator= (std::nullptr_t) noexcept
  {
    unique_ptr t{ nullptr };
    swap (*this, t);
    return *this;
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
    unique_ptr t{ p };
    swap (*this, t);
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
    return static_cast<base_type &> (*this);
  }

  const deleter_type &
  get_deleter () const noexcept
  {
    return static_cast<const base_type &> (*this);
  }

  friend void
  swap (unique_ptr &a, unique_ptr &b) noexcept
  {
    using std::swap;
    swap (static_cast<base_type &> (a), static_cast<base_type &> (b));
    swap (a.p_, b.p_);
  }

private:
  pointer p_;
};

#endif // UNIQUE_PTR_H
