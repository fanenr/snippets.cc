#ifndef UNIQUE_PTR_H
#define UNIQUE_PTR_H

#include <cstddef>
#include <type_traits>
#include <utility>

template <typename T>
class default_delete
{
public:
  static_assert (sizeof (T) > 0, "Cannot delete an incomplete type.");

  default_delete () noexcept = default;

  template <typename Y, typename = typename std::enable_if<
			    std::is_convertible<Y *, T *>::value>::type>
  default_delete (const default_delete<Y> &) noexcept
  {
  }

  void
  operator() (T *ptr) const
  {
    delete ptr;
  }
};

template <typename T, typename Deleter,
	  typename
	  = decltype (std::declval<Deleter> () (static_cast<T *> (0)))>
class delete_wrapper
{
  template <typename Y, typename D, typename>
  friend class delete_wrapper;

public:
  delete_wrapper (Deleter del) : del_ (std::move (del)) {}

  template <typename Y, typename D,
	    typename = typename std::enable_if<
		std::is_convertible<Y *, T *>::value
		&& std::is_convertible<D, Deleter>::value>::type>
  delete_wrapper (const delete_wrapper<Y, D> &other) noexcept
      : del_ (other.del_)
  {
  }

  template <typename Y, typename D,
	    typename = typename std::enable_if<
		std::is_convertible<Y *, T *>::value
		&& std::is_convertible<D, Deleter>::value>::type>
  delete_wrapper (delete_wrapper<Y, D> &&other) noexcept
      : del_ (std::move (other.del_))
  {
  }

  operator Deleter &() { return del_; }
  operator const Deleter &() const { return del_; }

  void
  operator() (T *ptr)
  {
    del_ (ptr);
  }

private:
  Deleter del_;
};

template <typename T, typename Deleter,
	  typename DeleterType =
	      typename std::conditional<std::is_class<Deleter>::value, Deleter,
					delete_wrapper<T, Deleter>>::type,
	  typename
	  = decltype (std::declval<DeleterType> () (static_cast<T *> (0)))>
using unique_ptr_base = DeleterType;

template <typename T, typename Deleter = default_delete<T>>
class unique_ptr : private unique_ptr_base<T, Deleter>
{
  template <typename Y, typename D>
  friend class unique_ptr;

  using base_type = unique_ptr_base<T, Deleter>;

public:
  using pointer = T *;
  using element_type = T;
  using deleter_type = Deleter;

  unique_ptr () noexcept : base_type (), ptr_ () {}
  unique_ptr (std::nullptr_t) noexcept : base_type (), ptr_ () {}

  explicit unique_ptr (pointer ptr) : base_type (), ptr_ (ptr) {}
  unique_ptr (pointer ptr, deleter_type del)
      : base_type (std::move (del)), ptr_ (ptr)
  {
  }

  ~unique_ptr ()
  {
    if (ptr_)
      static_cast<base_type &> (*this) (ptr_);
  }

  unique_ptr (const unique_ptr &) = delete;
  unique_ptr &operator= (const unique_ptr &) = delete;

  unique_ptr (unique_ptr &&other) noexcept
      : base_type (std::move (other)), ptr_ (other.ptr_)
  {
    other.ptr_ = {};
  }

  unique_ptr &
  operator= (unique_ptr &&other) noexcept
  {
    unique_ptr (std::move (other)).swap (*this);
    return *this;
  }

  template <typename Y, typename D,
	    typename = typename std::enable_if<
		std::is_convertible<typename unique_ptr<Y, D>::pointer,
				    pointer>::value
		&& std::is_convertible<typename unique_ptr<Y, D>::deleter_type,
				       deleter_type>::value>::type>
  unique_ptr (unique_ptr<Y, D> &&other) noexcept
      : base_type (std::move (other)), ptr_ (other.ptr_)

  {
    other.ptr_ = {};
  }

  template <typename Y, typename D,
	    typename = typename std::enable_if<
		std::is_convertible<typename unique_ptr<Y, D>::pointer,
				    pointer>::value
		&& std::is_convertible<typename unique_ptr<Y, D>::deleter_type,
				       deleter_type>::value>::type>
  unique_ptr &
  operator= (unique_ptr<Y, D> &&other) noexcept
  {
    unique_ptr (std::move (other)).swap (*this);
    return *this;
  }

  void
  swap (unique_ptr &other) noexcept
  {
    using std::swap;
    swap (static_cast<base_type &> (*this), static_cast<base_type &> (other));
    swap (ptr_, other.ptr_);
  }

  element_type &
  operator* () const
  {
    return *ptr_;
  }

  pointer
  operator->() const noexcept
  {
    return ptr_;
  }

  explicit
  operator bool () const noexcept
  {
    return ptr_;
  }

  pointer
  get () const noexcept
  {
    return ptr_;
  }

  void
  reset (pointer ptr = {}) noexcept
  {
    unique_ptr (ptr).swap (*this);
  }

  pointer
  release () noexcept
  {
    pointer ptr = ptr_;
    ptr_ = {};
    return ptr;
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
  pointer ptr_;
};

#endif // UNIQUE_PTR_H
