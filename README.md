# The Timestamp Lock

*NOTE*: I'm still finishing the write up and testing!

This is the implementation of a "timestamp lock", a type of lock I came up with. It may exist under another name, not sure.

It solves the problem of deadlocks caused by processes (or threads) crashing in the critical section. Whith this solution, locks are automatically released when the process crashes, furthermore the next process to acquire the lock will be notified that the previous holder died.

Regular locks use an atomic flag to encode if the lock was acquired. The "timestamp lock" uses a 64 bit word that is a date in the future when locked, and zero or a date in the past when unlocked.

Processes entering the critical section write atomically the timestamp when the acquire will expire. The timeout can be arbitrarily postponed by writing timestamps further away in the future. When leaving the critical section, 0 is written in the word.

If a process crashes inside the critical section, the lock will expire. The next process entering the section will detect the crash by finding an expired timestamp instead of 0.

Note that it's not really necessary for the timestamp to be UTC. It just needs to be synchronized between all partecipants. But, In the context of crashing processes, which causes relative clocks to be reset, using UTC is a good idea.