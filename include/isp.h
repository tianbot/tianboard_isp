#ifndef _ISP_H_
#define _ISP_H_

#include <stdio.h>
#include <unistd.h>

#define NONE    "\033[m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"

#define GREEN_REVERSE "\033[7;32m"

#define UP_LINE "\033[1A"
#define LEFT_1_C "\033[1D"
typedef struct
{
    unsigned char bootloader_version;
    unsigned char cmd_count;
    unsigned char cmd[16];
    unsigned int pid;
} stm32info_t;

int isp_init(const char *device, const int baud, const int databits, const int stopbits, const char parity, const int timeout); //成功返回1，失败返回0
void isp_close(void);

int isp_sync(void);
int isp_get_command(void);
int isp_get_pid(void);
int isp_erase_all(void);
int isp_write_bin(FILE *fp);
int isp_verify(FILE *fp);

void isp_reboot(void);
void isp_get_version(void);
void isp_go_app(void);

#endif
