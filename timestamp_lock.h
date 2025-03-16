typedef unsigned long long u64;

int   timestamp_trylock     (volatile u64 *word, u64 *ticket, int timeout_sec, int *crash);
int   timestamp_lock        (volatile u64 *word, int timeout_sec, int *crash);
void  timestamp_unlock      (volatile u64 *word, u64 *ticket);
int   timestamp_refreshlock (volatile u64 *word, u64 *ticket, int postpone_sec);
