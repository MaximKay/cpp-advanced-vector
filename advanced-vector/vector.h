#pragma once
#include <cassert>
#include <cstdlib>
#include <utility>
#include <memory>
#include <type_traits>
#include <iostream>
#include <algorithm>

template <typename T>
class RawMemory {
public:
	RawMemory() = default;

	explicit RawMemory(size_t capacity)
		: buffer_(Allocate(capacity))
		, capacity_(capacity) {
	}

	RawMemory(const RawMemory&) = delete;

	RawMemory& operator=(const RawMemory&) = delete;

	~RawMemory() {
		Deallocate(buffer_);
	}

	T* operator+(size_t offset) noexcept {
		// Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
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
		/*if (n <= 0) {
			return nullptr;
		};

		T* buf_ptr = nullptr;
		try {
			buf_ptr = static_cast<T*>(operator new(n * sizeof(T)));
		}
		catch (...) {
			Deallocate(buf_ptr);
			throw;
		}
		return buf_ptr;*/
	}

	// Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
	static void Deallocate(T* buf) noexcept {
		operator delete(buf);
	}

	T* buffer_ = nullptr;
	size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
	using iterator = T*;
	using const_iterator = const T*;

	Vector() = default;

	Vector(size_t size) : data_(size), size_(size) {
		std::uninitialized_value_construct_n(begin(), size);
	}

	Vector(const Vector& other) : data_(other.size_), size_(other.size_) {
		std::uninitialized_copy_n(other.begin(), other.size_, begin());
	}

	Vector(Vector&& other) noexcept {
		Swap(other);
	}

	Vector& operator=(const Vector& rhs) {
		if (this != &rhs) {
			if (rhs.size_ > data_.Capacity()) {
				Vector<T> new_vector(rhs);
				Swap(new_vector);
			}
			else {
				if (rhs.size_ < size_) {
					std::copy_n(rhs.begin(), rhs.size_, begin());
					std::destroy_n(begin() + rhs.size_, size_ - rhs.size_);
				}
				else {
					std::copy_n(rhs.begin(), size_, begin());
					std::uninitialized_copy_n(rhs.begin() + size_, rhs.size_ - size_, begin() + size_);
				};
				size_ = rhs.size_;
			};
		};

		return *this;
	}

	Vector& operator=(Vector&& rhs) noexcept {
		Swap(rhs);
		return *this;
	}

	iterator begin() noexcept {
		return data_.GetAddress();
	}

	iterator end() noexcept {
		return begin() + size_;
	}

	const_iterator begin() const noexcept {
		return cbegin();
	}

	const_iterator end() const noexcept {
		return cend();
	}

	const_iterator cbegin() const noexcept {
		return const_cast<const T*>(data_.GetAddress());
	}

	const_iterator cend() const noexcept {
		return const_cast<const T*>(begin() + size_);
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

	void Reserve(size_t capacity) {
		if (capacity <= data_.Capacity()) {
			return;
		};

		RawMemory<T> new_data(capacity);
		if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
			std::uninitialized_move_n(begin(), size_, new_data.GetAddress());
		}
		else {
			std::uninitialized_copy_n(begin(), size_, new_data.GetAddress());
		};

		std::destroy_n(begin(), size_);
		data_.Swap(new_data);
	}

	void Swap(Vector& other) noexcept {
		data_.Swap(other.data_);
		std::swap(size_, other.size_);
	}

	void Resize(size_t new_size) {
		if (new_size == size_) {
			return;
		};

		if (new_size < size_) {
			std::destroy_n(begin() + new_size, size_ - new_size);
		}
		else {
			Reserve(new_size);
			std::uninitialized_default_construct_n(end(), new_size - size_);
		};
		size_ = new_size;
	}

	void PushBack(const T& value) {
		EmplaceBack(std::forward<const T&>(value));
	}

	void PushBack(T&& value) {
		EmplaceBack(std::forward<T&&>(value));
	}

	template <typename... Args>
	T& EmplaceBack(Args&&... args) {
		if (size_ == Capacity()) {
			RawMemory<T> new_data((size_ == 0) ? 1 : size_ * 2);
			new (new_data + size_) T(std::forward<Args>(args)...);

			if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
				std::uninitialized_move_n(begin(), size_, new_data.GetAddress());
			}
			else {
				std::uninitialized_copy_n(begin(), size_, new_data.GetAddress());
			};

			data_.Swap(new_data);

			std::destroy_n(new_data.GetAddress(), size_);
		}
		else {
			new (data_ + size_) T(std::forward<Args>(args)...);
		};

		++size_;
		return data_[size_ - 1];
	}

	void PopBack() {
		std::destroy_at(end() - 1);
		--size_;
	}

	template <typename... Args>
	iterator Emplace(const_iterator pos, Args&&... args) {
		if (pos == cend()) {
			return &(EmplaceBack(std::forward<Args>(args)...));
		};

		const size_t index = std::distance(cbegin(), pos);

		if (size_ == Capacity()) {
			RawMemory<T> new_data((size_ == 0) ? 1 : size_ * 2);

			new (new_data.GetAddress() + index) T(std::forward<Args>(args)...);

			if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
				std::uninitialized_move_n(begin(), index, new_data.GetAddress());
				std::uninitialized_move_n((begin() + index), size_ - index, (new_data.GetAddress() + index + 1));
			}
			else {
				try {
					std::uninitialized_copy_n(begin(), index, new_data.GetAddress());
				}
				catch (...) {
					std::destroy_at(new_data.GetAddress() + index);
					throw;
				}

				try {
					std::uninitialized_copy_n((begin() + index), size_ - index, (new_data.GetAddress() + index + 1));
				}
				catch (...) {
					std::destroy_n(new_data.GetAddress(), index + 1);
					throw;
				}
			};

			std::destroy_n(begin(), size_);
			data_.Swap(new_data);
		}
		else {
			T temp(std::forward<Args>(args)...);
			new (data_ + size_) T(std::move(data_[size_ - 1]));
			std::move_backward(const_cast<T*>(pos), end() - 1, end());
			data_[index] = std::move(temp);
		};

		++size_;
		return &(data_[index]);
	}

	iterator Insert(const_iterator pos, const T& value) {
		return Emplace(pos, std::forward<const T&>(value));
	}

	iterator Insert(const_iterator pos, T&& value) {
		return Emplace(pos, std::forward<T&&>(value));
	}

	iterator Erase(const_iterator pos) {
		if (pos == (cend() - 1)) {
			PopBack();
			return end();
		};

		const size_t index = std::distance(cbegin(), pos);
		std::move(&(data_[index + 1]), end(), &(data_[index]));
		std::destroy_at(end() - 1);
		--size_;
		return &(data_[index]);
	}

	~Vector() noexcept {
		std::destroy_n(begin(), size_);
	}

private:
	RawMemory<T> data_{};
	size_t size_ = 0;
};
