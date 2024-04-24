#pragma once

#include <cstdint>
#include <cstring>
#include <memory>

class AVAudioBuffer {
 public:
  AVAudioBuffer() = default;
  AVAudioBuffer(size_t size)
      : size_(size),
        data_(new uint8_t[size], std::default_delete<uint8_t[]>()) {}

  void fill(void* p, size_t size) {
    if (p)
      memcpy(data_.get() + offset(), p, size);
    else
      memset(data_.get() + offset(), 0, size);
  }
  void fill(size_t offset, void* p, size_t size) {
    if (p)
      memcpy(data_.get() + offset, p, size);
    else
      memset(data_.get() + offset, 0, size);
  }
  void clear() {
    memset(data_.get(), 0, size_);
    offset_ = 0;
  }
  void reset(size_t newSize) {
    if (size_ < newSize) {
      data_.reset(new uint8_t[newSize], std::default_delete<uint8_t[]>());
      offset_ = 0;
      size_ = newSize;
    } else {
      clear();
    }
  }
  void skip(size_t n) { offset_ += n; }

  uint8_t* peek() const { return data_.get() + offset_; }
  size_t remain() const { return size_ - offset_; }
  size_t offset() const { return offset_; }
  size_t size() const { return size_; }

 private:
  size_t offset_{0};
  size_t size_;
  size_t capacity_;
  std::shared_ptr<uint8_t> data_;
};
