Juan Diego Becerra (jdb9056@nyu.edu)  
Professor John Sterling  
CS 3393  
April 7, 2024
___
# Issues
To ensure proper memory management and avoid memory leaks, I compile the program using the `-fsanitize=address`. Spuriously, executing `./shell` would cause the following error:

```txt
AddressSanitizer:DEADLYSIGNAL
AddressSanitizer:DEADLYSIGNAL
AddressSanitizer:DEADLYSIGNAL
AddressSanitizer:DEADLYSIGNAL
AddressSanitizer:DEADLYSIGNAL
AddressSanitizer:DEADLYSIGNAL
[1]    236615 segmentation fault (core dumped)  ./shell
```

Upon several attempts to debug this issue, I was not able to replicate this behavior. Even running the same set of commands that at one point caused this issue would not replicate it. It is worth noting that this issue did not arise during the final testing phase. However, since I was not able to identify its cause nor replicate it, I thought it was relevant to mention it here.

# Note
Professor Sterling, 

I want to note that I submitted a previous version of the program before the deadline. That version, unfortunately, had several shortcomings in terms of error reporting and memory management that complicated the debugging significantly. I have since revised these elements extensively in the current submission.

Lastly, I want to take this opportunity to thank you for the generous extension. It truly alleviated a lot of pressure from the current workload that I am managing during this final semester and allowed me to properly work on this assignment (even though I am submitting this late). I just did not feel comfortable submitting what I would considered to be a mediocre program, even if it implies taking the late penalty.
