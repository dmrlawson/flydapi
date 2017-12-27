/*
Code to generate DSHOT protocol signals from a Raspberry Pi GPIO output.

The assembly busy-loop delay, wait_cycles, was inspired by ElderBug's answer
here:
https://stackoverflow.com/questions/32719767/cycles-per-instruction-in-delay-loop-on-arm

I used this to help make the assembly code work:
https://www.cl.cam.ac.uk/projects/raspberrypi/tutorials/os/troubleshooting.html#immediate

The method of controlling the GPIO pins directly was taken from here:
https://elinux.org/RPi_GPIO_Code_Samples#Direct_register_access
*/


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

#define BCM2708_PERI_BASE        0x3F000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

int  mem_fd;
void *gpio_map;

// I/O access
volatile unsigned *gpio;

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH

#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock

//#define GPIO_PIN 21
#define GPIO_PIN 19

#define ARM_HIGH 1400

// Minimum -1600
// Maximum -400
#define OFFSET -1

#define SLEEP 2

// times 1.12
// dshot150 (THIS WORKS)

//const int T0H = 2800 + OFFSET; // 2500L - OFFSET;
//const int T0L = 4680 + OFFSET; // 4180L - OFFSET;
//const int T1H = 5600 + OFFSET; //5000L - OFFSET;
//const int T1L = 1880 + OFFSET; //1680L;



// dshot150-2 (MAIN SUCCESSES)
//const int T0H = 2980 + OFFSET; // 2500L - OFFSET;
//const int T0L = 4983 + OFFSET; // 4180L - OFFSET;
//const int T1H = 5961 + OFFSET; //5000L - OFFSET;
//const int T1L = 2003 + OFFSET; //1680L;


/*
// dshot300
const int T0H = 1490 - OFFSET; // 2500L - OFFSET;
const int T0L = 2491 - OFFSET; // 4180L - OFFSET;
const int T1H = 2980 - OFFSET; //5000L - OFFSET;
const int T1L = 1001 - OFFSET; //1680L;
*/

/*
//dshot600
const int T0H = 745 - OFFSET; // 2500L - OFFSET;
const int T0L = 1245 - OFFSET; // 4180L - OFFSET;
const int T1H = 1490 - OFFSET; //5000L - OFFSET;
const int T1L = 500 - OFFSET; //1680L;
*/

// dshot150 from graph
const int T0H = 2848 + OFFSET; // 2500L - OFFSET;
const int T0L = 4870 + OFFSET; // 4180L - OFFSET;
const int T1H = 5857 + OFFSET; //5000L - OFFSET;
const int T1L = 1861 + OFFSET; //1680L;


//const int DELAY = 10080 - OFFSET; //10000
//const int DELAY = 11200 - OFFSET; //10000
//const int DELAY = 22400; //10000
const int DELAY = 24000; //10000
//const int DELAY = 44800; //10000
//const int DELAY = 4000; // DSHOT600

inline void wait_cycles( int l ) {
    asm volatile( "0:" "SUBS %[count], #1;" "BNE 0b;" :[count]"+r"(l) );
}

inline void low(void) {
	GPIO_SET = 1<<GPIO_PIN;
	wait_cycles(T0H);
	GPIO_CLR = 1<<GPIO_PIN;
	wait_cycles(T0L);
}

inline void high(void) {
	GPIO_SET = 1<<GPIO_PIN;
	wait_cycles(T1H);
	GPIO_CLR = 1<<GPIO_PIN;
	wait_cycles(T1L);
}

inline void interpacket_delay(void) {
	GPIO_CLR = 1<<GPIO_PIN;
	wait_cycles(DELAY);
}

inline void interpacket_delay2(void) {
	GPIO_CLR = 1<<GPIO_PIN;
	usleep(SLEEP);
}

inline int add_checksum_and_telemetry(int packet, int telem) {
    int packet_telemetry = (packet << 1) | (telem & 1);
    int i;
    int csum = 0;
    int csum_data = packet_telemetry;
    for (i = 0; i < 3; i++) {
        csum ^=  csum_data;   // xor data by nibbles
        csum_data >>= 4;
    }
    csum &= 0xf;
    //csum = 0;
    // append checksum
    int packet_telemetry_checksum = (packet_telemetry << 4) | csum;

    return packet_telemetry_checksum;
}

inline void send_throttle(int throttle, int telem) {
	int throttle_packet = add_checksum_and_telemetry(throttle, telem);
	int array_index = 0;
	int i;
	for (i = 15; i >= 0; i--) {
		if ((throttle_packet >> i) & 1) {
			high();
		}
		else {
			low();
		}
	}
}

void setup_io();

inline void send_throttle_repeat(int value, int telemetry, int repeats) {
    int i = 0;
    for (i = 0; i < repeats; i++) {
        send_throttle(value, telemetry);
        interpacket_delay2();
        interpacket_delay2();
    }
}

int main(int argc, char **argv)
{
    int g;
    int rep;
    int i;
	int j;
    int throttle;

    // Set up gpi pointer for direct register access
    setup_io();

    INP_GPIO(GPIO_PIN); // must use INP_GPIO before we can use OUT_GPIO
    OUT_GPIO(GPIO_PIN);

    GPIO_CLR = 1<<GPIO_PIN;
    printf("Resetting\n");

//    sleep(1);
    send_throttle_repeat(0, 0, 30000);
//    send_throttle_repeat(20, 1, 100);
//    send_throttle_repeat(19, 1, 100);

    throttle = 48;
    printf("Throttle = %d\n", throttle);
    send_throttle_repeat(throttle, 0, 30000);
/*
    throttle = 58;
    printf("Throttle = %d\n", throttle);
    send_throttle_repeat(throttle, 0, 30000);
    
    throttle = 68;
    printf("Throttle = %d\n", throttle);
    send_throttle_repeat(throttle, 0, 30000);
    
    throttle = 78;
    printf("Throttle = %d\n", throttle);
    send_throttle_repeat(throttle, 0, 30000);
    
    throttle = 88;
    printf("Throttle = %d\n", throttle);
    send_throttle_repeat(throttle, 0, 30000);
*/    
    throttle = 298;
    printf("Throttle = %d\n", throttle);
    send_throttle_repeat(throttle, 0, 30000);

    throttle = 48;
    printf("Throttle = %d\n", throttle);
    send_throttle_repeat(throttle, 0, 30000);

    throttle = 500;
    printf("Throttle = %d\n", throttle);
    send_throttle_repeat(throttle, 0, 30000);

    throttle = 48;
    printf("Throttle = %d\n", throttle);
    send_throttle_repeat(throttle, 0, 300000);
    
    /*
    printf("going up\n");
    for (i = 0; i < 1000; i++) {
        send_throttle(i + 48, 0);
        interpacket_delay2();
        interpacket_delay2();
    }
    */

    //printf("Throttle = 1\n");
    //send_throttle_repeat(49, 0, 50000);

    /*
    for (i = 48; i < 200; i++) {
        printf("%d\n", i);
        send_throttle_repeat(i, 0, 2000);
    }
    */

    printf("Stopping\n", i);
    send_throttle_repeat(0, 0, 10);
/*
    printf("A\n");
    for (i = 1048; i < 1249; i++) {
        send_throttle_repeat(i, 0, 1000);
    }
    printf("B\n");
    for (i = 1249; i >= 1048; i--) {
        send_throttle_repeat(i, 0, 1000);
    }
    printf("C\n");
*/
//    send_throttle_repeat(1249, 0, 1000);
//    send_throttle_repeat(0, 0, 1000);

/*
    for (i = 0; i < 20; i++) {
        send_throttle(20, 1);
        interpacket_delay();
    }

    for (i = 0; i < 20; i++) {
        send_throttle(19, 1);
        interpacket_delay();
    }

    for (i = 0; i < 2000; i++) {
        send_throttle(0, 0);
        interpacket_delay();
    }
*/


/*
    printf("LOW (going up)\n");
    for (i = 48; i < 1048; i++) {
        for (j = 0; j < 100; j++) {
            send_throttle(i, 0);
            interpacket_delay();
        }
    }
*/



    /*for (i = 0; i < 1000; i++) {
        send_throttle(1048, 0);
        interpacket_delay();
    }
    for (i = 0; i < 1000; i++) {
        send_throttle(48, 0);
        interpacket_delay();
    }

    /*
    for (i = 1048; i < ARM_HIGH; i++) {
        for (j = 0; j < 1000; j++) {
            send_throttle(i, 0);
            interpacket_delay();
        }
    }


    for (i = 0; i < 1000; i++) {
        send_throttle(ARM_HIGH, 0);
        interpacket_delay();
    }
    printf("MID (going down)\n");
    for (i = ARM_HIGH; i >= 1048; i--) {
        for (j = 0; j < 1000; j++) {
            send_throttle(i, 0);
            interpacket_delay();
        }
    }

    /*
    printf("MID (going down)\n");
    for (i = 2047; i >= 1048; i--) {
        for (j = 0; j < 100; j++) {
            send_throttle(i, 0);
            interpacket_delay();
        }
    }
    */

/*
    printf("ARMED. RUNNING\n");
    for (i = 1049; i < 1600; i++) {
        for (j = 0; j < 50; j++) {
            send_throttle(i, 0);
            interpacket_delay();
        }
    }

    printf("RUNNING SOME MORE\n");
    for (i = 1600; i >= 1049; i--) {
        for (j = 0; j < 50; j++) {
            send_throttle(i, 0);
            interpacket_delay();
        }
    }
*/

//    printf("SLOW\n");
//    for (i = 0; i < 1000; i++) {
//            send_throttle(3, 1);
//            interpacket_delay();
//    }
//
//    printf("RESETING\n");
//    for (i = 0; i < 10000; i++) {
//            send_throttle(0, 0);
//            interpacket_delay();
//    }

  return 0;

} // main



//
// Set up a memory regions to access GPIO
//
void setup_io()
{
   /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      exit(-1);
   }

   /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      GPIO_BASE         //Offset to GPIO peripheral
   );

   close(mem_fd); //No need to keep mem_fd open after mmap

   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }

   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;


} // setup_io
