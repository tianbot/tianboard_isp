#include <isp.h>
#include <wiringSerial.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

static int fd;
static stm32info_t stm32info;

static int wait_ack(void)
{
    while (1)
    {
        if (0 != serialDataAvail(fd))
        {
            if (0x79 == serialGetchar(fd))
            {
                return 0;
            }
            else
            {
                return -1;
            }
        }
        usleep(1000);
    }
}

static unsigned char checksum(unsigned char *data, int len)
{
    int i;
    unsigned char cs;
    cs = 0;
    for (i = 0; i < len; i++)
    {
        cs ^= data[i];
    }
    return cs;
}

static int isp_write_block(unsigned char *data, unsigned int addr, int len)
{
    unsigned char temp[4];
    unsigned char len1;
    int i;
    temp[0] = ((addr >> 24) & 0xff);
    temp[1] = ((addr >> 16) & 0xff);
    temp[2] = ((addr >> 8) & 0xff);
    temp[3] = ((addr)&0xff);

    serialPutchar(fd, 0x31);
    serialPutchar(fd, 0xce);
    wait_ack();

    serialPutchar(fd, temp[0]);
    serialPutchar(fd, temp[1]);
    serialPutchar(fd, temp[2]);
    serialPutchar(fd, temp[3]);
    serialPutchar(fd, checksum(temp, 4));
    wait_ack();

    len1 = (unsigned char)(len - 1);
    serialPutchar(fd, len1);
    for (i = 0; i < len; i++)
    {
        serialPutchar(fd, data[i]);
    }
    serialPutchar(fd, len1 ^ checksum(data, len));
    wait_ack();

    serialFlush(fd);
    usleep(1000);
    return 0;
}

static int isp_read_block(unsigned char *data, unsigned int addr, int len)
{
    unsigned char temp[4];
    unsigned char len1;
    int i;
    temp[0] = ((addr >> 24) & 0xff);
    temp[1] = ((addr >> 16) & 0xff);
    temp[2] = ((addr >> 8) & 0xff);
    temp[3] = ((addr)&0xff);

    serialPutchar(fd, 0x11);
    serialPutchar(fd, 0xEE);
    wait_ack();
    serialPutchar(fd, temp[0]);
    serialPutchar(fd, temp[1]);
    serialPutchar(fd, temp[2]);
    serialPutchar(fd, temp[3]);
    serialPutchar(fd, checksum(temp, 4));
    wait_ack();

    len1 = (unsigned char)(len - 1);
    serialPutchar(fd, len1);
    serialPutchar(fd, ~len1);
    wait_ack();

    for (i = 0; i < len; i++)
    {
        data[i] = serialGetchar(fd);
    }
    return 0;
}

int isp_init(const char *device, const int baud, const int databits, const int stopbits, const char parity, const int timeout)
{
    fd = serialOpen(device, baud, databits, stopbits, parity, timeout);
    if (fd != -1)
    {
        return 0;
    }
    else
    {
        printf(RED "dev %s open failed\n" NONE, device);
        return -1;
    }
}

void isp_close()
{
    serialClose(fd);
    usleep(100000);
}

void isp_reboot()
{
    printf("reboot ");

    int i;
    char buf[] = "update";
    unsigned int len;
    unsigned short cmd = 0x4000;
    unsigned char bcc = 0;

    serialPutchar(fd, 0x55);
    serialPutchar(fd, 0xAA);

    len = 2 + strlen(buf);

    serialPutchar(fd, len & 0xFF);
    serialPutchar(fd, (len >> 8) & 0xFF);

    serialPutchar(fd, cmd & 0xFF);
    serialPutchar(fd, (cmd >> 8) & 0xFF);
    bcc ^= cmd & 0xFF;
    bcc ^= (cmd >> 8) & 0xFF;
    for (i = 0; i < strlen(buf); i++)
    {
        serialPutchar(fd, buf[i]);
        bcc ^= buf[i];
    }

    serialPutchar(fd, bcc);
    usleep(1000000);
    printf(GREEN "[done]\n" NONE);
}

char *get_response(unsigned char *buff, unsigned int data_len)
{
    static char response[128];
    static uint8_t state = 0;
    uint8_t *p = buff;
    static uint32_t len;
    static uint32_t offset;
    uint8_t bcc = 0;
    uint32_t i;

    while (data_len != 0)
    {
        switch (state)
        {
        case 0:
            if (*p == 0x55)
            {
                len = 0;
                state = 1;
                offset = 0;
            }
            break;

        case 1:
            if (*p == 0xaa)
            {
                state = 2;
            }
            else
            {
                state = 0;
            }
            break;

        case 2: // len
            len = *p;
            state = 3;
            break;

        case 3: // len
            len += (*p) * 256;
            if (len > 1024 * 10)
            {
                state = 0;
                break;
            }
            state = 4;
            break;

        case 4: // pack_type
            if (*p == 0x00)
            {
                state = 5;
                len--;
            }
            else
            {
                state = 0;
            }
            break;

        case 5: // pack_type
            if (*p == 0xC0)
            {
                state = 6;
                len--;
            }
            else
            {
                state = 0;
            }
            break;

        case 6: //data
            if (len--)
            {
                response[offset++] = (*p);
            }
            else
            {
                bcc = 0;
                state = 0;
                bcc ^= 0x00;
                bcc ^= 0xC0;
                for (i = 0; i < offset; i++)
                {
                    bcc ^= response[i];
                }

                if (bcc == *p)
                {
                    return response;
                }
            }
            break;

        default:
            state = 0;
            break;
        }
        data_len--;
        p++;
    }
    return NULL;
}

char *isp_get_version(void)
{
    int i = 0;
    int recv_cnt = 20;
    char send_buf[32] = "version";
    char recv_buf[8192];
    unsigned int len;
    unsigned short cmd = 0x4000;
    unsigned char bcc = 0;
    char *response = NULL;
    serialFlush(fd);
    do
    {
        serialPutchar(fd, 0x55);
        serialPutchar(fd, 0xAA);

        len = 2 + strlen(send_buf);

        serialPutchar(fd, len & 0xFF);
        serialPutchar(fd, (len >> 8) & 0xFF);

        serialPutchar(fd, cmd & 0xFF);
        serialPutchar(fd, (cmd >> 8) & 0xFF);
        bcc ^= cmd & 0xFF;
        bcc ^= (cmd >> 8) & 0xFF;
        for (i = 0; i < strlen(send_buf); i++)
        {
            serialPutchar(fd, send_buf[i]);
            bcc ^= send_buf[i];
        }

        serialPutchar(fd, bcc);

        usleep(50000);
        int len = serialDataAvail(fd);
        if (len != 0)
        {
            i = 0;

            while ((len--) && (i < sizeof(recv_buf)))
            {
                recv_buf[i] = serialGetchar(fd);
                i++;
            }
            response = get_response(recv_buf, i);
            if (response != NULL)
            {
                break;
            }
        }
        recv_cnt--;
    } while ((i < sizeof(recv_buf)) && recv_cnt);
    return response;
}

int isp_sync()
{
    printf("sync ");
    usleep(100000);
    serialFlush(fd);
    while (1)
    {
        serialPutchar(fd, 0x7f);
        if (wait_ack() == 0)
        {
            printf(GREEN "[done]\n" NONE);
            return 0;
        }
        usleep(10000);
    }
}

int isp_get_command(void)
{
    unsigned char get[32];
    int i, j;
    serialPutchar(fd, 0x00);
    serialPutchar(fd, 0xff);
    wait_ack();

    i = 0;
    while (1)
    {
        get[i] = serialGetchar(fd);
        if (get[i] == 0x79)
        {
            break;
        }
        else if (get[i] == 0x1f)
        {
            return -1;
        }
        i++;
    }

    stm32info.bootloader_version = get[1];
    stm32info.cmd_count = get[0];
    for (j = 0; j < stm32info.cmd_count; j++)
    {
        stm32info.cmd[j] = get[j + 2];
    }

    serialFlush(fd);

    printf("bootloader version: " GREEN "[0x%02x]\n" NONE, stm32info.bootloader_version);
    printf("commands support: " GREEN "[");
    for (i = 0; i < stm32info.cmd_count; i++)
    {
        printf("0x%02x ", stm32info.cmd[i]);
    }
    printf(LEFT_1_C "]" NONE "\n");
    return 0;
}

int isp_get_pid(void)
{
    unsigned char get[16];
    int i, j;
    serialPutchar(fd, 0x02);
    serialPutchar(fd, 0xfd);
    wait_ack();

    serialGetchar(fd);
    stm32info.pid = serialGetchar(fd);
    stm32info.pid <<= 8;
    stm32info.pid += serialGetchar(fd);
    wait_ack();

    serialFlush(fd);

    printf("pid: " GREEN "[0x%04x]\n" NONE, stm32info.pid);

    return 0;
}

int isp_erase_all()
{
    int i;

    printf("erasing flash ");
    fflush(stdout);

    for (i = 0; i < stm32info.cmd_count; i++)
    {
        if (stm32info.cmd[i] == 0x43)
        {
            serialPutchar(fd, 0x43);
            serialPutchar(fd, 0xbc);
            wait_ack();
            serialPutchar(fd, 0xff);
            serialPutchar(fd, 0x00);
            wait_ack();
            break;
        }
        if (stm32info.cmd[i] == 0x44)
        {
            serialPutchar(fd, 0x44);
            serialPutchar(fd, 0xbb);
            wait_ack();
            serialPutchar(fd, 0xff);
            serialPutchar(fd, 0xff);
            serialPutchar(fd, 0x00);
            wait_ack();
            break;
        }
    }

    serialFlush(fd);
    printf(GREEN "[done]\n" NONE);
    return 0;
}

int isp_write_bin(FILE *fp)
{
    unsigned char buf[256];
    int i = 0, offset = 0;
    int read_size;
    int filesize;

    fseek(fp, 0L, SEEK_END);
    filesize = ftell(fp);
    rewind(fp);
    printf("filesize %d \r\n", filesize);
    printf("download %3d%%\n", offset * 100 / filesize);

    while (1)
    {
        read_size = fread(buf, 1, filesize - offset > 256 ? 256 : filesize - offset, fp);
        isp_write_block(buf, 0x08000000 + offset, 256);
        offset += read_size;
        printf(UP_LINE "download %3d%%\n", offset * 100 / filesize);
        if (offset >= filesize)
        {
            break;
        }
    }
    return 0;
}

int isp_verify(FILE *fp)
{
    unsigned char buf[256];
    unsigned char buf_read[256];
    int i = 0, j = 0, offset = 0;
    int filesize;

    fseek(fp, 0L, SEEK_END);
    filesize = ftell(fp);
    rewind(fp);

    printf("verify %3d%%\n", offset * 100 / filesize);

    while (1)
    {
        fread(&buf[i], 1, 1, fp);
        if (feof(fp))
        {
            break;
        }
        i++;
        if (i == 256)
        {
            isp_read_block(buf_read, 0x08000000 + offset, 256);
            for (j = 0; j < 256; j++)
            {
                if (buf[j] != buf_read[j])
                {
                    printf(RED "verify fail\n" NONE);
                    return -1;
                }
            }
            printf(UP_LINE "verify %3d%%\n", offset * 100 / filesize);
            i = 0;
            offset += 256;
        }
    }

    isp_read_block(buf_read, 0x08000000 + offset, i);
    for (j = 0; j < i; j++)
    {
        if (buf[i] != buf_read[i])
        {
            printf(RED "verify fail\n" NONE);
            return -1;
        }
    }
    offset += i;
    printf(UP_LINE "verify %3d%% " GREEN "[done]\n" NONE, offset * 100 / filesize);
    return 0;
}

void isp_go_app(void)
{
    printf("run app ");
    serialPutchar(fd, 0x21);
    serialPutchar(fd, 0xde);
    wait_ack();
    serialPutchar(fd, 0x08);
    serialPutchar(fd, 0x00);
    serialPutchar(fd, 0x00);
    serialPutchar(fd, 0x00);
    serialPutchar(fd, 0x08);
    printf(GREEN "[done]\n" NONE);
}
