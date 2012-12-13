#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <usb.h>
#include <errno.h>

/*
 * Temper.c by Robert Kavaler (c) 2009 (relavak.com)
 * All rights reserved.
 *
 * Modified by Sylvain Leroux (c) 2012 (sylvain@chicoree.fr)
 *
 * Temper driver for linux. This program can be compiled either as a library
 * or as a standalone program (-DUNIT_TEST). The driver will work with some
 * TEMPer usb devices from RDing (www.PCsensor.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Robert kavaler BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include "comm.h"

struct Product { 
	uint16_t	vendor;
	uint16_t	id;
	const char	*name;
};

static const struct Product ProductList[] = {
/*
	Not supported: commands are different
	{
		0x1130, 0x660c,
		"Original RDing TEMPer"
	},
*/
	{
		/* Analog Device ADT75 (or similar) based device */
		/* with two temperature sensors (internal & external) */
		0x0c45, 0x7401,
		"RDing TEMPer2V1.3"
	},
	{
		/* Sensirion SHT1x based device */
		/* with internal humidity & temperature sensor */
		0x0c45, 0x7402,
		"RDing TEMPerHumiV1.1"
	},
};
static const unsigned ProductCount = sizeof(ProductList)/sizeof(struct Product);

struct Temper {
	struct usb_device *device;
	usb_dev_handle *handle;
	int debug;
	int timeout;
	const struct Product	*product;
};

Temper *
TemperCreate(struct usb_device *dev, int timeout, int debug, const struct Product* product)
{
	Temper *t;
	int ret;

	if (debug) {
		printf("Temper device %s (%04x:%04x)\n",
			product->name,
			product->vendor,
			product->id);
	}

	t = calloc(1, sizeof(*t));
	t->device = dev;
	t->debug = debug;
	t->product = product;
	t->timeout = timeout;
	t->handle = usb_open(t->device);
	if(!t->handle) {
		free(t);
		return NULL;
	}
	if(t->debug) {
		printf("Trying to detach kernel driver\n");
	}

	ret = usb_detach_kernel_driver_np(t->handle, 0);
	if(ret) {
		if(errno == ENODATA) {
			if(t->debug) {
				printf("Device already detached\n");
			}
		} else {
			if(t->debug) {
				printf("Detach failed: %s[%d]\n",
				       strerror(errno), errno);
				printf("Continuing anyway\n");
			}
		}
	} else {
		if(t->debug) {
			printf("detach successful\n");
		}
	}
	ret = usb_detach_kernel_driver_np(t->handle, 1);
	if(ret) {
		if(errno == ENODATA) {
			if(t->debug)
				printf("Device already detached\n");
		} else {
			if(t->debug) {
				printf("Detach failed: %s[%d]\n",
				       strerror(errno), errno);
				printf("Continuing anyway\n");
			}
		}
	} else {
		if(t->debug) {
			printf("detach successful\n");
		}
	}

	if(usb_set_configuration(t->handle, 1) < 0 ||
	   usb_claim_interface(t->handle, 0) < 0 ||
	   usb_claim_interface(t->handle, 1)) {
		usb_close(t->handle);
		free(t);
		return NULL;
	}
	return t;
}

Temper *
TemperCreateFromDeviceNumber(int deviceNum, int timeout, int debug)
{
	struct usb_bus *bus;
	int n;

	n = 0;
	for(bus=usb_get_busses(); bus; bus=bus->next) {
	    struct usb_device *dev;

	    for(dev=bus->devices; dev; dev=dev->next) {
		if(debug) {
			printf("Found device: %04x:%04x\n",
			       dev->descriptor.idVendor,
			       dev->descriptor.idProduct);
		}
		for(unsigned i = 0; i < ProductCount; ++i) {	
			if(dev->descriptor.idVendor == ProductList[i].vendor &&
			   dev->descriptor.idProduct == ProductList[i].id) {
				if(debug) {
				    printf("Found deviceNum %d\n", n);
				}
				if(n == deviceNum) {
					return TemperCreate(dev, timeout,
							 debug, 
							 &ProductList[i]);
				}
				n++;
			}
		}
	    }
	}
	return NULL;
}

void
TemperFree(Temper *t)
{
	if(t) {
		if(t->handle) {
			usb_close(t->handle);
		}
		free(t);
	}
}

int
TemperSendCommand8(Temper *t, int a, int b, int c, int d, int e, int f, int g, int h)
{
	unsigned char buf[8+8*8];
	int ret;

	bzero(buf, sizeof(buf));
	buf[0] = a;
	buf[1] = b;
	buf[2] = c;
	buf[3] = d;
	buf[4] = e;
	buf[5] = f;
	buf[6] = g;
	buf[7] = h;

	if(t->debug) {
		printf("sending bytes %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x (buffer len = %d)\n",
		       a, b, c, d, e, f, g, h, sizeof(buf));
	}

	ret = usb_control_msg(t->handle, 0x21, 9, 0x200, 0x01,
			    (char *) buf, sizeof(buf), t->timeout);

	if(ret != sizeof(buf)) {
		perror("usb_control_msg failed");
		return -1;
	}
	return 0;
}

int
TemperSendCommand2(Temper *t, int a, int b)
{
	unsigned char buf[8+8*8];
	int ret;

	bzero(buf, sizeof(buf));
	buf[0] = a;
	buf[1] = b;

	if(t->debug) {
		printf("sending bytes %02x, %02x (buffer len = %d)\n",
		       a, b, sizeof(buf));
	}

	ret = usb_control_msg(t->handle, 0x21, 9, 0x201, 0x00,
			    (char *) buf, sizeof(buf), t->timeout);

	if(ret != sizeof(buf)) {
		perror("usb_control_msg failed");
		return -1;
	}
	return 0;
}

int TemperInterruptRead(Temper* t, unsigned char *buf, unsigned int len) {
	int ret;

	if (t->debug) {
		printf("interrupt read\n");
	}

	ret = usb_interrupt_read(t->handle, 0x82, (char*)buf, len, t->timeout);
	if(t->debug) {
		printf("receiving %d bytes\n",ret);
		for(int i = 0; i < ret; ++i) {
			printf("%02x ", buf[i]);
			if ((i+1)%8 == 0) printf("\n");
		}
		printf("\n");
        }

	return ret;
}

static float
TemperByteToCelcius(Temper* t, uint8_t high, uint8_t low) {
	int16_t word = ((int8_t)high << 8) | low;

#if 0
	word += t->offset; /* calibration value */
#endif
	
	return ((float)word) * (125.0 / 32000.0);
}

int
TemperGetData(Temper *t, struct TemperData *data) {
	unsigned char buf[8];
	int ret = TemperInterruptRead(t, buf, sizeof(buf));

	data->tempA = TemperByteToCelcius(t, buf[2], buf[3]);
	data->tempB = TemperByteToCelcius(t, buf[4], buf[5]);
	
	return ret;
}

/*
static int
TemperGetData(Temper *t, char *buf, int len)
{
//	int ret;

	return usb_control_msg(t->handle, 0xa1, 1, 0x300, 0x01,
			    (char *) buf, len, t->timeout);
}
*/
/*
int
TemperGetTemperatureInC(Temper *t, float *tempC)
{
	char buf[256];
	int ret, temperature, i;

	TemperSendCommand(t, 10, 11, 12, 13, 0, 0, 2, 0);
	TemperSendCommand(t, 0x54, 0, 0, 0, 0, 0, 0, 0);
	for(i = 0; i < 7; i++) {
		TemperSendCommand(t, 0, 0, 0, 0, 0, 0, 0, 0);
	}
	TemperSendCommand(t, 10, 11, 12, 13, 0, 0, 1, 0);
	ret = TemperGetData(t, buf, 256);

	if(t->debug) {
		printf("receiving %d bytes\n",ret);
		for(int i = 0; i < ret; ++i) {
			printf("%02x ", buf[i]);
			if ((i+1)%8 == 0) printf("\n");
		}
		printf("\n");
        }


	if(ret < 2) {
		return -1;
	}

	temperature = (buf[1] & 0xFF) + (buf[0] << 8);	
	temperature += 1152;			// calibration value
	*tempC = temperature * (125.0 / 32000.0);
	return 0;
}
*/
/*
int
TemperGetOtherStuff(Temper *t, char *buf, int length)
{
	TemperSendCommand(t, 10, 11, 12, 13, 0, 0, 2, 0);
	TemperSendCommand(t, 0x52, 0, 0, 0, 0, 0, 0, 0);
	TemperSendCommand(t, 10, 11, 12, 13, 0, 0, 1, 0);
	return TemperGetData(t, buf, length);
}
*/
