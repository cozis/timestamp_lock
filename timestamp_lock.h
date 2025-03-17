typedef unsigned long long u64;

// Tries acquiring the lock for a number of seconds
//
// Arguments:
//   word       : The lock's word
//   ticket     : Token needed to unlock the critical section
//   timeout_sec: Number of seconds after which the lock will
//                be automatically released
//   crash      : If the lock is acquired, the pointer value is
//                set to 1 if the previous holder of the lock
//                crashed, else it's set to 0.
// Returns:
//   0 on success or a negated error code:
//     EAGAIN: Section is locked
//     EAGAIN: Couldn't read the time
int timestamp_trylock(volatile u64 *word, u64 *ticket, int timeout_sec, int *crash);

// Acquires the lock for a number of seconds. Unlike trylock, if
// the lock is acquired this function waits.
//
// Arguments:
//   Same as trylock
//
// Returns:
//   0 on success or a negated error code:
//     EAGAIN: WaitOnAddress failed
//   plus any error code returned by futex or timestamp_trylock
int timestamp_lock(volatile u64 *word, u64 *ticket, int timeout_sec, int *crash);

// Unlocks the critical section gracefully
//
// Arguments:
//   word: The lock's word
//   ticket: The ticket value returned by trylock/lock when locking
//
// Returns:
//   0 on success or a negated error code:
//     ETIMEDOUT: The lock was already unlocked due to the expiration
//   plus any error code returned by futex.
int timestamp_unlock(volatile u64 *word, u64  ticket);

// Postpones the timeout of the currently acquired lock
//
// Arguments:
//   word        : The lock's word
//   ticket      : Value to be passed to the next unlock call.
//                 This substitutes the value returned by lock.
//   postpone_sec: Number of seconds the timeout must be moved
//                 into the future
//
// Returns:
//   0 on success or a negated error code:
//   EAGAIN: Couldn't read the time
//   ETIMEDOUT: The lock was realeased due to the expiration
int timestamp_refreshlock(volatile u64 *word, u64 *ticket, int postpone_sec);
