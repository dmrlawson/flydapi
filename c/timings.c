/*
Testing the timer code to find out what values I need to use to generate the
DSHOT signals for the ESCs.

The assembly busy-loop delay, wait_cycles, was inspired by ElderBug's answer
here:
https://stackoverflow.com/questions/32719767/cycles-per-instruction-in-delay-loop-on-arm

The time_cycles function was inspired by Basile Starynkevitch's answer here:
https://stackoverflow.com/questions/16275444/how-to-print-time-difference-in-accuracy-of-milliseconds-and-nanoseconds
*/

#include <stdio.h>
#include <time.h>
#include <unistd.h>

inline void wait_cycles(int l)
{
    asm volatile( "0:" "SUBS %[count], #1;" "BNE 0b;" :[count]"+r"(l) );
}

int time_cycles(int cycles)
{
    struct timespec tm;
    struct timespec tstart={0,0}, tend={0,0};
    long double timing;
    long double min = 999999999;
    int i;

    for(i = 0; i < 10000000; i++)
    {
        clock_gettime(CLOCK_MONOTONIC, &tstart);
        wait_cycles(cycles);
        clock_gettime(CLOCK_MONOTONIC, &tend);

        timing = ((long double)tend.tv_sec*1.0e9 + tend.tv_nsec)
         - ((long double)tstart.tv_sec*1.0e9 + tstart.tv_nsec);

        if (timing < min) {
            min = timing;
        }
    }
    return min;
}

int main()
{
        int i;
        int new_time;
        int last_time = 0;

        for(i = 1; i < 1000; i++)
        {
            new_time = time_cycles(i);
            if (new_time != last_time) {
                printf("%d cycles = %d ns\n", i, new_time);
                last_time = new_time;
            }
        }

        return 0;
}
