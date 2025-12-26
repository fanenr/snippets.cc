#ifndef SPSC_RING_BUFFER_H
#define SPSC_RING_BUFFER_H

#include <atomic>
#include <memory>
#include <new>

template <typename T, size_t Capacity>
class spsc_ring_buffer
{
public:
  using size_type = size_t;

  spsc_ring_buffer ()
      : head_ (0), tail_ (0), buffer_ (std::make_unique<T[]> (Capacity + 1))
  {
  }

  spsc_ring_buffer (const spsc_ring_buffer &) = delete;
  spsc_ring_buffer &operator= (const spsc_ring_buffer &) = delete;

  template <typename U>
  bool
  push (U &&item)
  {
    size_type curr_head = head_.load (std::memory_order_relaxed);
    size_type next_head = (curr_head + 1) % (Capacity + 1);

    if (next_head == tail_.load (std::memory_order_acquire))
      return false;

    buffer_[curr_head] = std::forward<U> (item);
    head_.store (next_head, std::memory_order_release);

    return true;
  }

  bool
  pop (T &elem)
  {
    size_type curr_tail = tail_.load (std::memory_order_relaxed);

    if (curr_tail == head_.load (std::memory_order_acquire))
      return false;

    elem = buffer_[curr_tail];
    tail_.store ((curr_tail + 1) % (Capacity + 1), std::memory_order_release);

    return true;
  }

  size_type
  size () const
  {
    size_type curr_head = head_.load (std::memory_order_relaxed);
    size_type curr_tail = tail_.load (std::memory_order_relaxed);

    if (curr_head >= curr_tail)
      return curr_head - curr_tail;
    else
      return (Capacity + 1) - (curr_tail - curr_head);
  }

  bool
  is_full () const
  {
    return (head_.load (std::memory_order_relaxed) + 1) % (Capacity + 1)
	   == tail_.load (std::memory_order_relaxed);
  }

  bool
  is_empty () const
  {
    return head_.load (std::memory_order_relaxed)
	   == tail_.load (std::memory_order_relaxed);
  }

private:
  std::atomic<size_type> head_;
  unsigned char pad_[64 - sizeof (head_)];
  std::atomic<size_type> tail_;

  std::unique_ptr<T[]> buffer_;
};

#endif // SPSC_RING_BUFFER_H
