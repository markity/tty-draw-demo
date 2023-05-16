typedef struct bitmapfileheader
{
    unsigned short bfType;                    // 0x4d42固定值, 标识文件类型
    unsigned int bfSize;                      // 整个文件的大小
    unsigned short bfReserved1;               // 保留字段, 必须为0
    unsigned short bfReserved2;               // 保留字段, 必须为0
    unsigned int bfOffBits;                   // 从文件头到位图数据的偏移, 单位字节, 这里的偏移其实也可以说是位图数据相对整个文件起始位置的偏移
} __attribute__((packed)) bitmapfileheader_t; // 要求编译器不对此结构体进行内存对齐

typedef struct bitmapinfoheader
{
    unsigned int biSize;         // 此结构体的大小 (14-17字节)
    int biWidth;                 // 图像的宽  (18-21字节)
    int biHeight;                // 图像的高  (22-25字节)
    unsigned short biPlanes;     // 表示bmp图片的平面属，显然显示器只有一个平面，所以恒等于1 (26-27字节)
    unsigned short biBitCount;   // 一像素所占的位数   (28-29字节)
    unsigned int biCompression;  // 说明图象数据压缩的类型，0为不压缩。 (30-33字节)
    unsigned int biSizeImage;    // 像素数据所占大小, 这个值应该等于上面文件头结构中bfSize-bfOffBits (34-37字节)
    int biXPelsPerMeter;         // 说明水平分辨率，用像素/米表示。一般为0 (38-41字节)
    int biYPelsPerMeter;         // 说明垂直分辨率，用像素/米表示。一般为0 (42-45字节)
    unsigned int biClrUsed;      // 说明位图实际使用的调色板中的颜色索引数（设为0的话，则说明使用所有调色板项）。 (46-49字节)
    unsigned int biClrImportant; // 说明对图象显示有重要影响的颜色索引的数目，如果是0，表示都重要。(50-53字节)
} __attribute__((packed)) bitmapinfoheader_t;

#include <ft2build.h>
#include <fcntl.h>
#include <memory.h>
#include <unistd.h>
#include <freetype2/freetype/ftcolor.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

FT_Library library;
FT_Face face;

int main() {
    FT_Error error = FT_Init_FreeType(&library);
    if (error) {
        printf("failed to FT_Init_FreeType\n");
        return 1;
    }


    int major, minor;
    FT_Library_Version(library, &major, &minor, NULL);
    printf("version: %d.%d\n", major, minor);

    error = FT_New_Face(library, "./font.ttc", 0, &face);
    if (error) {
        printf("failed to FT_New_Face\n");
        return 1;
    }

    int size = 64;
    error = FT_Set_Pixel_Sizes(face, 0, size);
    if (error) {
        printf("font size %d not supported\n", size);
        return 1;
    }

    FT_UInt glyphIdx = FT_Get_Char_Index(face, L'我');
    error = FT_Load_Glyph(face, glyphIdx, 0);
    if (glyphIdx == 0) {
        printf("glyph not found\n");
        return 1;
    }

    printf("glyph index: %d\n", glyphIdx);

    error = FT_Load_Glyph(face, glyphIdx, FT_LOAD_COLOR|FT_LOAD_DEFAULT);
    if (error) {
        printf("failed to load glyph\n");
        return 1;
    }

    FT_Glyph glyph;
    FT_Get_Glyph(face->glyph, &glyph);
    FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, NULL, 1);

    FT_BitmapGlyph glyphBitmap = (FT_BitmapGlyph)glyph;

    FT_Bitmap bitmap = glyphBitmap->bitmap;

    printf("height(px): %d width(px): %d pitch: %d\n", bitmap.rows, bitmap.width, bitmap.pitch);

    //      ---

    int bufferSize = 4 * bitmap.rows * bitmap.width;
    int outputfileSize = sizeof(bitmapfileheader_t) + sizeof(bitmapinfoheader_t) + bufferSize;
    unsigned char *outputBytes = malloc(outputfileSize);
    if (outputBytes == NULL) {
        perror("failed to malloc mem");
        return 1;
    }
    printf("file size: %d\n", outputfileSize);
    printf("image buffer size: %d\n", bufferSize);

    // 写文件头
    bitmapfileheader_t fheader = {};
    fheader.bfType = 0x4d42;
    fheader.bfSize = outputfileSize;
    fheader.bfOffBits = sizeof(bitmapfileheader_t) + sizeof(bitmapinfoheader_t);
    memcpy(outputBytes, &fheader, sizeof(bitmapfileheader_t));

    // 写图像信息头
    bitmapinfoheader_t iheader = {};
    iheader.biBitCount = 32;
    iheader.biHeight = -bitmap.rows;
    iheader.biWidth = bitmap.width;
    iheader.biPlanes = 1;
    iheader.biSize = sizeof(bitmapinfoheader_t);
    iheader.biSizeImage = bufferSize;
    memcpy(outputBytes + sizeof(bitmapfileheader_t), &iheader, sizeof(bitmapinfoheader_t));

    // 将8bit灰度图片转换为rgb格式
    for (int y = 0; y < bitmap.rows; y++) {
        for (int x = 0; x < bitmap.width; x++) {
            unsigned grayScale = bitmap.buffer[y * bitmap.pitch + x];
            if (grayScale) {
                *((outputBytes + sizeof(bitmapfileheader_t) + sizeof(bitmapinfoheader_t) + y * bitmap.width*4 + x*4 + 0)) = grayScale;
                *((outputBytes + sizeof(bitmapfileheader_t) + sizeof(bitmapinfoheader_t) + y * bitmap.width*4 + x*4 + 1)) = grayScale;
                *((outputBytes + sizeof(bitmapfileheader_t) + sizeof(bitmapinfoheader_t) + y * bitmap.width*4 + x*4 + 2)) = grayScale;
                *((outputBytes + sizeof(bitmapfileheader_t) + sizeof(bitmapinfoheader_t) + y * bitmap.width*4 + x*4 + 3)) = 0;
            }
        }
    }

    int outputFd = open("ft2_output.bmp", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (outputFd < 0) {
        perror("failed to create file");
        return 1;
    }

    if (write(outputFd, outputBytes, outputfileSize) == -1) {
        perror("failed to write to file");
        return 1;
    }

    return 0;
}