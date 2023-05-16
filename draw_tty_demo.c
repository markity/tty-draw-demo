#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>
#include <ncurses.h>
#include <linux/tty.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <ncurses.h>
#include <linux/vt.h>

/*
    屏幕的坐标规定:

    (0,0)------------------x轴
    |
    |
    |
    |
    |
    |
    y轴
*/

int main()
{
    // 打开fb设备文件
	int framebuffer_fd = open("/dev/fb0", O_RDWR);
	if (framebuffer_fd == -1) {
	    perror("failed to open framebuffer device");
	    exit(1);
	}
	printf("Framebuffer device opened successfully.\n");

	// 获取屏幕信息, var -> variable, 是可修改的, 可变的信息
    struct fb_var_screeninfo vinfo;
	if (ioctl(framebuffer_fd, FBIOGET_VSCREENINFO, &vinfo)) {
	    perror("failed to read variable information");
	    exit(1);
	}
	printf("%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    // 每个像素都会是4个字节表示它的颜色
	if (vinfo.bits_per_pixel != 32) {
	    printf("not supported bits_per_pixel, it only supports 32 bit color");
	    exit(1);
	}

    // 屏幕的大小=x像素数目*y像素数目
	int screen_size = vinfo.xres * vinfo.yres;

	// framebuffer的设备实现了映射到内存的接口, 因此这里需要用mmap
    // 参数1: NULL, 起始位置由系统指定, 一般mmap都将它设置为NULL
    // 参数2: 映射的字节数目, 一个像素4字节(32bit)
    // 参数3: 指定读/写/执行权限, 这里只需要读写
    // 参数4: 这块内存是否独享, 如果是private, 那么它的写不会同步到文件, 然而文件改变会刷新这块内存
    //      而shared则同步到文件
	volatile char* framebuffer_mem = (char *)mmap(NULL, screen_size*vinfo.bits_per_pixel/8, PROT_READ | PROT_WRITE, MAP_SHARED, framebuffer_fd, 0);
	if (framebuffer_mem == NULL) {
	    perror("failed to map framebuffer device to memory");
	    exit(1);
	}
	printf("The framebuffer device was mapped to memory successfully.\n");
    printf("Press Enter to show new screen:\n");

    if (ioctl(STDIN_FILENO, KDSETMODE, KD_GRAPHICS) < 0) {
        perror("failed to set tty to graphics mode");
        exit(1);
    }

    initscr();
    raw();
    noecho();
    refresh();
    keypad(stdscr, TRUE);

    vinfo.activate |= FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
    if(ioctl(framebuffer_fd, FBIOPUT_VSCREENINFO, &vinfo) < 0) {
        perror("failed to set framebuffer mode");
        exit(1);
    }

    int c = getch();
    if (c != '\n') {
        goto end;
    }

    for (int j = 0; j < 5; j ++) {
        for (size_t i = 0; i < screen_size; i++) {
            if(j % 2 == 0) {
                *(framebuffer_mem + i*4 + 0) = 0xff;
                *(framebuffer_mem + i*4 + 1) = 0x00;
                *(framebuffer_mem + i*4 + 2) = 0x00;
                *(framebuffer_mem + i*4 + 3) = 0x00;
            } else {
                *(framebuffer_mem + i*4 + 0) = 0x00;
                *(framebuffer_mem + i*4 + 1) = 0xff;
                *(framebuffer_mem + i*4 + 2) = 0x00;
                *(framebuffer_mem + i*4 + 3) = 0x00;
            }
        }
        usleep(1000000);
    }
    
    end:

	munmap((void*)framebuffer_mem, screen_size*4);
	close(framebuffer_fd);

    ioctl(STDIN_FILENO, KDSETMODE, KD_TEXT);

    endwin();

	return 0;
}