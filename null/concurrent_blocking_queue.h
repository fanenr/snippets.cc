#ifndef CONCURRENT_BLOCKING_QUEUE_H
#define CONCURRENT_BLOCKING_QUEUE_H

#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <optional>
#include <utility>

template <typename T>
class concurrent_blocking_queue
{
public:
  using element_type = T;
  using size_type = size_t;

  explicit concurrent_blocking_queue (size_type capacity
				      = std::numeric_limits<size_type>::max ())
      : capacity_ (capacity), closed_ (false)
  {
  }

  concurrent_blocking_queue (const concurrent_blocking_queue &) = delete;
  concurrent_blocking_queue &operator= (const concurrent_blocking_queue &)
      = delete;

  template <typename U>
  bool
  push (U &&elem)
  {
    std::unique_lock<std::mutex> lock (mutex_);
    not_full_cv_.wait (lock, [this] ()
			 { return queue_.size () < capacity_ || closed_; });

    if (closed_)
      return false;

    queue_.push_back (std::forward<U> (elem));
    not_empty_cv_.notify_one ();

    return true;
  }

  template <typename U, typename Duration>
  bool
  try_push (U &&elem, const Duration &timeout)
  {
    std::unique_lock<std::mutex> lock (mutex_);
    bool success = not_full_cv_.wait_for (
	lock, timeout,
	[this] () { return queue_.size () < capacity_ || closed_; });

    if (!success || closed_)
      return false;

    queue_.push_back (std::forward<U> (elem));
    not_empty_cv_.notify_one ();

    return true;
  }

  std::optional<T>
  pop ()
  {
    std::unique_lock<std::mutex> lock (mutex_);
    not_empty_cv_.wait (lock, [this] { return !queue_.empty () || closed_; });

    if (queue_.empty () && closed_)
      return std::nullopt;

    auto elem = std::move (queue_.front ());
    queue_.pop_front ();
    not_full_cv_.notify_one ();

    return elem;
  }

  template <typename Duration>
  std::optional<T>
  try_pop (const Duration &timeout)
  {
    std::unique_lock<std::mutex> lock (mutex_);
    bool success = not_empty_cv_.wait_for (
	lock, timeout, [this] { return !queue_.empty () || closed_; });

    if (!success || (queue_.empty () && closed_))
      return std::nullopt;

    auto elem = std::move (queue_.front ());
    queue_.pop_front ();
    not_full_cv_.notify_one ();

    return elem;
  }

  void
  close ()
  {
    {
      std::lock_guard<std::mutex> lock (mutex_);
      closed_ = true;
    }
    not_empty_cv_.notify_all ();
    not_full_cv_.notify_all ();
  }

  size_type
  size () const
  {
    std::lock_guard<std::mutex> lock (mutex_);
    return queue_.size ();
  }

  bool
  empty () const
  {
    std::lock_guard<std::mutex> lock (mutex_);
    return queue_.empty ();
  }

  bool
  is_closed () const
  {
    std::lock_guard<std::mutex> lock (mutex_);
    return closed_;
  }

private:
  size_type capacity_;
  std::deque<T> queue_;
  mutable std::mutex mutex_;

  bool closed_;
  std::condition_variable not_full_cv_;
  std::condition_variable not_empty_cv_;
};

#endif // CONCURRENT_BLOCKING_QUEUE_H
