#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <isp.h>

int main(int argc, char *argv[])
{
    FILE *fp = NULL;

    int i;

    if (argc != 3)
    {
        printf(YELLOW "Usage: tianboard_isp [serial dev] [firmware bin file]\n");
        printf(GREEN "Example: tianboard_isp /dev/ttyUSB0 racecar_1.0.0.bin\n" NONE);
        return -1;
    }

    if (isp_init(argv[1], 115200, 8, 1, 'N', 30) != 0)
    {
        return -1;
    }

    fp = fopen(argv[2], "rb");
    if (fp == NULL)
    {
        printf(RED "file %s open failed\n" NONE, argv[2]);
        isp_close();
        return -1;
    }

    //isp_get_version();

    isp_reboot();

    isp_close();

    if (isp_init(argv[1], 115200, 8, 1, 'E', 30) != 0)
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

    if (isp_init(argv[1], 115200, 8, 1, 'N', 30) != 0)
    {
        fclose(fp);
        return -1;
    }

    //isp_get_version();

    isp_close();
    fclose(fp);

    printf(GREEN_REVERSE "ALL DONE!\n" NONE);
    return 0;
}
