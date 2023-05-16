#define _GNU_SOURCE

#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <memory.h>
    
typedef struct bitmapfileheader {
    unsigned short          bfType;             // 0x4d42固定值, 标识文件类型
    unsigned int            bfSize;             // 整个文件的大小
    unsigned short          bfReserved1;        // 保留字段, 必须为0
    unsigned short          bfReserved2;        // 保留字段, 必须为0
    unsigned int            bfOffBits;          // 从文件头到位图数据的偏移, 单位字节, 这里的偏移其实也可以说是位图数据相对整个文件起始位置的偏移
} __attribute__((packed)) bitmapfileheader_t;   // 要求编译器不对此结构体进行内存对齐

typedef struct bitmapinfoheader {
    unsigned int    biSize;          // 此结构体的大小 (14-17字节)
    int             biWidth;         // 图像的宽  (18-21字节)
    int             biHeight;        // 图像的高  (22-25字节)
    unsigned short  biPlanes;        // 表示bmp图片的平面属，显然显示器只有一个平面，所以恒等于1 (26-27字节)
    unsigned short  biBitCount;      // 一像素所占的位数   (28-29字节)
    unsigned int    biCompression;   // 说明图象数据压缩的类型，0为不压缩。 (30-33字节)
    unsigned int    biSizeImage;     // 像素数据所占大小, 这个值应该等于上面文件头结构中bfSize-bfOffBits (34-37字节)
    int             biXPelsPerMeter; // 说明水平分辨率，用像素/米表示。一般为0 (38-41字节)
    int             biYPelsPerMeter; // 说明垂直分辨率，用像素/米表示。一般为0 (42-45字节)
    unsigned int    biClrUsed;       // 说明位图实际使用的调色板中的颜色索引数（设为0的话，则说明使用所有调色板项）。 (46-49字节)
    unsigned int    biClrImportant;  // 说明对图象显示有重要影响的颜色索引的数目，如果是0，表示都重要。(50-53字节)
} __attribute__((packed)) bitmapinfoheader_t;

int main(int argc, char **argv) {
    if(argc != 2) {
        printf("usage: %s <file_name>\n", argv[0]);
        return 1;
    }

    int fbdev = open("/dev/fb0", O_RDONLY, 0);
    if (fbdev == -1) {
        perror("failed to open fb0 device");
        return 1;
    }

    struct fb_var_screeninfo vinfo;
	if (ioctl(fbdev, FBIOGET_VSCREENINFO, &vinfo)) {
	    perror("failed to read variable information");
	    return 1;
	}

	if (vinfo.bits_per_pixel != 32) {
	    printf("not supported bits_per_pixel, it only supports 32 bit color");
	    return 1;
	}

    
    char* framebuffer_mem = malloc(vinfo.xres*vinfo.yres*vinfo.bits_per_pixel/8);
    if (framebuffer_mem == NULL) {
        perror("failed to malloc mem");
        return 1;
    }

	if (read(fbdev, framebuffer_mem, vinfo.xres*vinfo.yres*vinfo.bits_per_pixel/8) < 0) {
	    perror("failed to read framebuffer device to memory");
	    return 1;
	}

    printf("%d %d %d\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    // 输出文件由三部分组成
    int outputfileSize = sizeof(bitmapfileheader_t) + sizeof(bitmapinfoheader_t) + vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    char *outputBytes = malloc(outputfileSize);
    if (outputBytes == NULL) {
        perror("failed to malloc mem");
        return 1;
    }
    printf("大小: %d\n", outputfileSize);

    // 写文件头
    bitmapfileheader_t fheader = {};
    fheader.bfType = 0x4d42;
    fheader.bfSize = sizeof(bitmapfileheader_t) + sizeof(bitmapinfoheader_t) + vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    fheader.bfOffBits = sizeof(bitmapfileheader_t) + sizeof(bitmapinfoheader_t);
    memcpy(outputBytes, &fheader, sizeof(bitmapfileheader_t));

    // 写图像信息头
    bitmapinfoheader_t iheader = {};
    iheader.biBitCount = vinfo.bits_per_pixel;
    iheader.biHeight = -vinfo.yres;
    iheader.biWidth = vinfo.xres;
    iheader.biPlanes = 1;
    iheader.biSize = sizeof(bitmapinfoheader_t);
    iheader.biSizeImage = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    memcpy(outputBytes+sizeof(bitmapfileheader_t), &iheader, sizeof(bitmapinfoheader_t));

    // 拷贝raw数据
    memcpy(outputBytes+sizeof(bitmapfileheader_t)+sizeof(bitmapinfoheader_t), framebuffer_mem, vinfo.xres*vinfo.yres*vinfo.bits_per_pixel/8);

    int outputFd = open(argv[1], O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (outputFd < 0) {
        perror("failed to create file");
        return 1;
    }

    if(write(outputFd, outputBytes, outputfileSize) == -1) {
        perror("failed to write to file");
        return 1;
    }

    return 0;
}