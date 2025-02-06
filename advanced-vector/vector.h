#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

#define MOVE_OR_COPY_IF_NOEXCEPT(old_begin, new_begin, count) \
    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) { \
        std::uninitialized_move_n((old_begin), (count), (new_begin)); \
    } else { \
        std::uninitialized_copy_n((old_begin), (count), (new_begin)); \
    }

template <typename T>
class RawMemory;

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(other.size_)
    {
        other.size_ = 0;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector tmp(rhs);
                Swap(tmp);
            } else {
                T* left = data_.GetAddress();
                const T* right = rhs.data_.GetAddress();

                if (rhs.size_ > size_) {
                    std::copy(right, right + size_, left);
                    std::uninitialized_copy(right + size_, right + rhs.size_, left + size_);
                } else {
                    std::copy(right, right + rhs.size_, left);
                    std::destroy_n(left + rhs.size_, size_ - rhs.size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_ = std::move(rhs.data_);
            size_ = rhs.size_;
            rhs.size_ = 0;
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        T* new_begin = new_data.GetAddress();
        T* old_begin = data_.GetAddress();

        MOVE_OR_COPY_IF_NOEXCEPT(old_begin, new_begin, size_);

        std::destroy_n(old_begin, size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size > data_.Capacity()) {
            Reserve(new_size);
        }

        if (new_size > size_) {
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        } else {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }

        size_ = new_size;
    }

    void PushBack(const T& elem) {
        EmplaceBack(elem);
    }

    void PushBack(T&& elem) {
        EmplaceBack(std::move(elem));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... elem) {
        return *Emplace(end(), std::forward<Args>(elem)...);
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert(pos >= begin() && pos <= end());
        size_t insert_index = static_cast<size_t>(pos - begin());
        
        if (size_ < data_.Capacity()) {
            iterator insert_pos = begin() + insert_index;
            if (insert_index == size_) {
                std::construct_at(data_.GetAddress() + size_, std::forward<Args>(args)...);
            } else {
                T temp(std::forward<Args>(args)...);
                std::construct_at(data_.GetAddress() + size_, std::move(data_[size_ - 1]));
                std::move_backward(insert_pos, end() - 1, end());
                *insert_pos = std::move(temp);
            }
            ++size_;
            return begin() + insert_index;
        }

        RawMemory<T> new_data(size_ == 0 ? 1 : 2 * size_);
        T* new_begin = new_data.GetAddress();
        T* old_begin = data_.GetAddress();
        iterator new_insert_pos = new_begin + insert_index;

        MOVE_OR_COPY_IF_NOEXCEPT(old_begin, new_begin, insert_index);
        std::construct_at(new_insert_pos, std::forward<Args>(args)...);
        MOVE_OR_COPY_IF_NOEXCEPT(old_begin + insert_index, new_insert_pos + 1, size_ - insert_index);

        std::destroy_n(old_begin, size_);
        data_.Swap(new_data);
        ++size_;
        return new_insert_pos;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    iterator Erase(const_iterator pos) {
        assert(pos >= begin() && pos < end());
        iterator non_const_pos = const_cast<iterator>(pos);
        std::move(non_const_pos + 1, end(), non_const_pos);
        PopBack();
        return non_const_pos;
    }

    void PopBack() {
        assert(size_ > 0);
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        buffer_ = other.buffer_;
        capacity_ = other.capacity_;
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory&& other) noexcept {
        if (this != &other) {
            if (buffer_ != nullptr) {
                Deallocate(buffer_);
            }
            buffer_ = other.buffer_;
            capacity_ = other.capacity_;
            other.buffer_ = nullptr;
            other.capacity_ = 0;
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};
