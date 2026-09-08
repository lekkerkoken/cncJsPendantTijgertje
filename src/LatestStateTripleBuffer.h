#ifndef LATEST_STATE_TRIPLE_BUFFER_H
#define LATEST_STATE_TRIPLE_BUFFER_H

#include <atomic>
#include <cstdint>

// De Rust triple_buffer gebruikt precies het model dat hier goed past:
// één atomic byte met bufferindex + dirty-bit, waarbij producer en
// consumer elk hun eigen buffer hebben en alleen de gedeelde "back buffer"
// atomair wordt omgewisseld.

template<typename T>
class LatestStateTripleBuffer
{
public:

    LatestStateTripleBuffer();


    // ========================================================
    // PUBLISH
    // ========================================================
    //
    // Called by the single producer.
    //
    // Copies a complete value into the producer's private
    // buffer and publishes it as the latest available value.
    //
    // If the consumer is slower than the producer, intermediate
    // values may be overwritten. Only the latest value matters.
    //
    // ========================================================

    void publish(
        const T& value
    );


    // ========================================================
    // ACQUIRE
    // ========================================================
    //
    // Called by the single consumer.
    //
    // If a new value is available, switches to the newly
    // published buffer and copies the latest value into 'value'.
    //
    // If no new value is available, the consumer remains on
    // its current buffer, which is already the latest value
    // known to the consumer, and copies that value into 'value'.
    //
    // Return value:
    //
    //     true  = buffer was switched to a newly published value
    //     false = no new value was published
    //
    // In BOTH cases, 'value' contains the latest value available
    // to the consumer.
    //
    // ========================================================

    bool acquire(
        T& value
    ) const;


private:

    static constexpr uint8_t BUFFER_COUNT = 3;


    static constexpr uint8_t BUFFER_INDEX_MASK = 0b00000011;

    static constexpr uint8_t BUFFER_DIRTY_BIT = 0b00000100;


    // ========================================================
    // Storage
    // ========================================================

    T buffers_[BUFFER_COUNT];


    // ========================================================
    // Producer state
    // ========================================================
    //
    // Only the producer accesses writeIndex_.
    //
    // Therefore this does not need to be atomic.
    //
    // This is the buffer into which the producer is currently
    // allowed to write.
    //

    uint8_t writeIndex_;


    // ========================================================
    // Consumer state
    // ========================================================
    //
    // Only the consumer accesses readIndex_.
    //
    // Therefore this does not need to be atomic.
    //
    // This is the buffer from which the consumer is currently
    // reading.
    //
    // mutable because acquire() is logically a const read
    // operation, while advancing the consumer's private
    // read position is an internal implementation detail.
    //

    mutable uint8_t readIndex_;


    // ========================================================
    // Shared state
    // ========================================================
    //
    // Lower two bits:
    //
    //     index of the shared back/published buffer
    //
    // Third bit:
    //
    //     1 = a new value has been published
    //     0 = no new value since the consumer last acquired it
    //
    // This is the ONLY piece of state accessed by both producer
    // and consumer.
    //
    // The exchange operations on this atomic transfer ownership
    // of the shared buffer.
    //

    mutable std::atomic<uint8_t> backInfo_;
};


// ============================================================
// CONSTRUCTOR
// ============================================================

template<typename T>
LatestStateTripleBuffer<T>::LatestStateTripleBuffer()
    :
    writeIndex_(1),
    readIndex_(2),
    backInfo_(0)
{
}


// ============================================================
// PUBLISH
// ============================================================

template<typename T>
void LatestStateTripleBuffer<T>::publish(
    const T& value
)
{
    //
    // The producer exclusively owns writeIndex_.
    //
    // Therefore it is safe to modify this buffer without any
    // synchronization.
    //

    buffers_[writeIndex_] =
        value;


    //
    // The complete value has now been written.
    //
    // Publish our buffer as the new back buffer and mark it
    // dirty so the consumer knows that a new value is available.
    //
    // The previous back buffer becomes our new private write
    // buffer.
    //
    // AcqRel is important:
    //
    // Release:
    //     all writes to the buffer happen before publication.
    //
    // Acquire:
    //     when we receive the previous back buffer, we acquire
    //     ownership of that buffer before writing to it again.
    //

    uint8_t previousBackInfo =
        backInfo_.exchange(
            writeIndex_ |
            BUFFER_DIRTY_BIT,
            std::memory_order_acq_rel
        );


    //
    // The buffer that was previously the back buffer is now
    // exclusively ours.
    //

    writeIndex_ =
        previousBackInfo &
        BUFFER_INDEX_MASK;
}


// ============================================================
// ACQUIRE
// ============================================================

template<typename T>
bool LatestStateTripleBuffer<T>::acquire(
    T& value
) const
{
    //
    // First check whether the producer has published something
    // since our previous acquire().
    //
    // This relaxed load is only an optimization/check.
    // It does not itself transfer ownership.
    //

    uint8_t backInfo =
        backInfo_.load(
            std::memory_order_relaxed
        );


    bool updated =
        (
            (
                backInfo &
                BUFFER_DIRTY_BIT
            ) != 0
        );


    if (updated)
    {
        //
        // A new value exists.
        //
        // Exchange our current read buffer with the shared back
        // buffer.
        //
        // The buffer returned by the exchange is now exclusively
        // ours as the consumer.
        //
        // Our previous read buffer becomes the producer's new
        // private write buffer.
        //
        // AcqRel is required for both directions of ownership:
        //
        // Release:
        //     our previous read buffer must no longer be accessed
        //     before the producer is allowed to reuse it.
        //
        // Acquire:
        //     the newly published buffer must be completely visible
        //     before we read it.
        //

        uint8_t previousBackInfo =
            backInfo_.exchange(
                readIndex_,
                std::memory_order_acq_rel
            );


        //
        // The buffer that was previously published is now ours.
        //

        readIndex_ =
            previousBackInfo &
            BUFFER_INDEX_MASK;
    }


    //
    // Whether or not a new value was published, readIndex_ always
    // points to the latest buffer currently owned by the consumer.
    //
    // Therefore the caller always receives the latest value.
    //

    value =
        buffers_[readIndex_];


    //
    // Tell the caller whether the consumer actually switched
    // to a newly published buffer.
    //

    return updated;
}


#endif