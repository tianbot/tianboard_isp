#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <isp.h>
#include <stdint.h>

#define SW_VER "v1.0.0"

struct argument_struct
{
    uint32_t baudrate;
    uint32_t baudrate_new;
    char *file_name;
    char *serial;
    uint32_t reset_flag;
};

void help(void)
{
    printf("Usage:\r\n"
           "   -r              reset the board and enter FW update mode\r\n"
           "   -b baudrate     the serial baudrate of the current FW\r\n"
           "   -n baudrate     the serial baudrate of the new FW\r\n"
           "   -f bin file     the firmware bin file to be updated\r\n"
           "   -s serial port  the host serial port connected to the board\r\n"
           "   -v              version\r\n"
           "   -h              help\r\n"
           "\r\n");
}

int argument_parse(int argc, char *argv[], struct argument_struct *p_argument)
{
    char *arg;
    memset(p_argument, 0, sizeof(struct argument_struct));
    p_argument->baudrate = 460800;
    p_argument->baudrate_new = 460800;
    //p_argument->reset_flag = 1;
    p_argument->serial = "/dev/ttyUSB0";
    while (--argc)
    {
        arg = *++argv;

        if (*arg++ != '-')
        {
            continue;
        }

        switch (*arg++)
        {
        case 'r':
            //arg = *++argv;
            //--argc;
            p_argument->reset_flag = 1;
            break;

        case 'b':
            arg = *++argv;
            --argc;
            p_argument->baudrate = atoi(arg);
            break;

        case 'n':
            arg = *++argv;
            --argc;
            p_argument->baudrate_new = atoi(arg);
            break;

        case 'f':
            arg = *++argv;
            --argc;
            p_argument->file_name = arg;
            break;

        case 's':
            arg = *++argv;
            --argc;
            p_argument->serial = arg;
            break;

        case 'h':
            //arg = *++argv;
            //--argc;
            help();
            exit(0);

        case 'v':
            //arg = *++argv;
            //--argc;
            printf("tianboard_isp " SW_VER "\r\n");
            exit(0);

        default:
            printf(RED "invalid option '-%c'\r\n" NONE, *--arg);
            return -1;
        }
        if (argc == 0)
        {
            break;
        }
    }
    if (p_argument->file_name == NULL)
    {
        printf(RED "hex file needs to be specified\r\n" NONE);
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    FILE *fp = NULL;
    struct argument_struct argument;
    int i;
    if (argument_parse(argc, argv, &argument) != 0)
    {
        help();
        return -1;
    }

    fp = fopen(argument.file_name, "rb");
    if (fp == NULL)
    {
        printf(RED "file %s open failed\n" NONE, argument.file_name);
        isp_close();
        return -1;
    }

    if (argument.reset_flag) //if reset needed, send reset cmd to board by the specified baudrate
    {
        if (isp_init(argument.serial, argument.baudrate, 8, 1, 'N', 30) != 0)
        {
            return -1;
        }

        char *version = isp_get_version();

        if (version != NULL)
        {
            printf("Current FW version:\r\n");
            printf(GREEN "%s\r\n" NONE, version);
        }
        else
        {
            printf(RED "Get version failed, exit\r\n" NONE);
            exit(-1);
        }

        isp_reboot();

        isp_close();
    }

    if (isp_init(argument.serial, 115200, 8, 1, 'E', 30) != 0)
    {
        fclose(fp);
        return -1;
    }

    if (isp_sync() != 0)
    {
        fclose(fp);
        isp_close();
        return -1;
    }

    isp_get_command();

    isp_get_pid();

    isp_erase_all();

    isp_write_bin(fp);

    if (isp_verify(fp))
    {
        fclose(fp);
        isp_close();
        return -1;
    }

    isp_go_app();

    isp_close();

    printf("wait for board boot up...\r\n");
    sleep(6);

    if (isp_init(argument.serial, argument.baudrate_new, 8, 1, 'N', 30) != 0)
    {
        return -1;
    }
    char *version = isp_get_version();

    if (version != NULL)
    {
        printf("New FW version:\r\n");
        printf(GREEN "%s\r\n" NONE, version);
    }
    else
    {
        printf(RED "Get version failed, exit\r\n" NONE);
        exit(-1);
    }

    isp_close();
    fclose(fp);

    printf(GREEN_REVERSE "ALL DONE!\r\n" NONE);
    return 0;
}
