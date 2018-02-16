#include <Python.h>
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

#if PY_MAJOR_VERSION >= 3
#define PY3K
#endif


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

#define NUM_PINS 27
#define OFFSET 0
// dshot150 from graph
const int T0H = 2848 + OFFSET; // 2500L - OFFSET;
const int T0L = 4870 + OFFSET; // 4180L - OFFSET;
const int T1H = 5857 + OFFSET; //5000L - OFFSET;
const int T1L = 1861 + OFFSET; //1680L;


int pins[NUM_PINS] = {0};
int pin_num = 0;
int first_time = 1;



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


static PyObject *
dshot_send(PyObject *self, PyObject *args)
{
    int value;
    int pin;

    if (!PyArg_ParseTuple(args, "ii", &value, &pin))
        Py_RETURN_FALSE;

    if (first_time)
    {
        first_time = 0;
        setup_io();
    }
    if (!pins[pin])
    {
        pins[pin] = 1;
        INP_GPIO(pin); // must use INP_GPIO before we can use OUT_GPIO
        OUT_GPIO(pin);
        GPIO_CLR = 1<<pin;
    }

    send_throttle(pin, value, 0);

    Py_RETURN_TRUE;
}

static PyMethodDef DshotMethods[] =
{
    {"send",  (PyCFunction)dshot_send, METH_VARARGS,
    "Execute a shell command."},
    {NULL, NULL, 0, NULL}    /* Sentinel */
};

static struct PyModuleDef dshotmodule = {
    PyModuleDef_HEAD_INIT,
    "dshot",   /* name of module */
    NULL, /* module documentation, may be NULL */
    -1,       /* size of per-interpreter state of the module,
                 or -1 if the module keeps state in global variables. */
    DshotMethods
};



inline void wait_cycles( int l ) {
    asm volatile( "0:" "SUBS %[count], #1;" "BNE 0b;" :[count]"+r"(l) );
}

inline void low(pin) {
	GPIO_SET = 1<<pin;
	wait_cycles(T0H);
	GPIO_CLR = 1<<pin;
	wait_cycles(T0L);
}

inline void high(pin) {
	GPIO_SET = 1<<pin;
	wait_cycles(T1H);
	GPIO_CLR = 1<<pin;
	wait_cycles(T1L);
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

inline void send_throttle(int pin, int throttle, int telem) {
	int throttle_packet = add_checksum_and_telemetry(throttle, telem);
	int array_index = 0;
	int i;
	for (i = 15; i >= 0; i--) {
		if ((throttle_packet >> i) & 1) {
			high(pin);
		}
		else {
			low(pin);
		}
	}
}

PyMODINIT_FUNC
PyInit_dshot(void)
{
    return PyModule_Create(&dshotmodule);
}


