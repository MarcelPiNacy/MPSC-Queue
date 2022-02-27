/*
Copyright (c) 2022 Marcel Pi Nacy

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/



#pragma once
#include <atomic>



// Global state for producers
template <typename T>
struct MPSCQueueProducer
{
    std::atomic<T*> tail = nullptr;
    std::atomic<T*> head = nullptr;

    void Push(T* ptr)
    {
        T* prior = tail.exchange(ptr, std::memory_order_acquire);
        if (prior == nullptr) // Publish new head:
            return head.store(ptr, std::memory_order_release);
        prior->next = ptr;
        std::atomic_thread_fence(std::memory_order_release);
    }
};

// Consumer-private struct
template <typename T>
struct MPSCQueueConsumer
{
    T* head;

    T* Pop(MPSCQueueProducer<T>& producer)
    {
        if (head == nullptr) // Query producer:
        {
            if (producer.head.load(std::memory_order_acquire) == nullptr)
                return nullptr;
            head = producer.head.exchange(nullptr, std::memory_order_acquire);
        }
        auto r = head;
        head = head->next;
        if (head == nullptr) // Presumably, we have run out of elements in the list, let's attempt to close it:
        {
            T* assumed_last = r;
            (void)producer.tail.compare_exchange_strong(assumed_last, nullptr, std::memory_order_release, std::memory_order_relaxed);
        }
        return r;
    }
};