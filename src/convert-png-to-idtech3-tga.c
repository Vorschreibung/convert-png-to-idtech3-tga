#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_le16(FILE *fp, uint16_t value) {
    uint8_t bytes[2];
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
    fwrite(bytes, sizeof(bytes), 1, fp);
}

static int load_png_rgba(const char *path, uint8_t **out_pixels, uint32_t *out_w,
                         uint32_t *out_h) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open input PNG: %s\n", path);
        return 0;
    }

    uint8_t sig[8];
    if (fread(sig, 1, sizeof(sig), fp) != sizeof(sig) ||
        png_sig_cmp(sig, 0, sizeof(sig)) != 0) {
        fprintf(stderr, "Input is not a valid PNG: %s\n", path);
        fclose(fp);
        return 0;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fprintf(stderr, "Failed to create libpng read struct.\n");
        fclose(fp);
        return 0;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        fprintf(stderr, "Failed to create libpng info struct.\n");
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return 0;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "libpng error while reading %s\n", path);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return 0;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, sizeof(sig));
    png_read_info(png_ptr, info_ptr);

    png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
    png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
    int color_type = png_get_color_type(png_ptr, info_ptr);
    int bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }

    if (!(color_type & PNG_COLOR_MASK_ALPHA) &&
        !png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    }

    png_read_update_info(png_ptr, info_ptr);

    png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    uint8_t *pixels = (uint8_t *)malloc(rowbytes * height);
    if (!pixels) {
        fprintf(stderr, "Out of memory while decoding PNG.\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return 0;
    }

    png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
    if (!row_pointers) {
        fprintf(stderr, "Out of memory while decoding PNG.\n");
        free(pixels);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return 0;
    }

    for (png_uint_32 y = 0; y < height; ++y) {
        row_pointers[y] = pixels + y * rowbytes;
    }

    png_read_image(png_ptr, row_pointers);
    png_read_end(png_ptr, NULL);

    free(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    if (rowbytes != width * 4) {
        fprintf(stderr, "Unexpected PNG row format.\n");
        free(pixels);
        return 0;
    }

    *out_pixels = pixels;
    *out_w = (uint32_t)width;
    *out_h = (uint32_t)height;
    return 1;
}

static int pixels_equal(const uint8_t *pixels, size_t a, size_t b, size_t bpp) {
    return memcmp(pixels + a * bpp, pixels + b * bpp, bpp) == 0;
}

static int write_tga_rle(const char *path, const uint8_t *rgba_top_left, uint32_t width,
                         uint32_t height) {
    if (width > 65535u || height > 65535u) {
        fprintf(stderr, "TGA supports up to 65535x65535 pixels.\n");
        return 0;
    }
    const size_t bpp = 4;
    const size_t pixel_count = (size_t)width * (size_t)height;
    uint8_t *pixels = (uint8_t *)malloc(pixel_count * bpp);
    if (!pixels) {
        fprintf(stderr, "Out of memory while preparing TGA.\n");
        return 0;
    }

    for (uint32_t y = 0; y < height; ++y) {
        uint32_t src_y = height - 1 - y;
        const uint8_t *src_row = rgba_top_left + (size_t)src_y * width * bpp;
        uint8_t *dst_row = pixels + (size_t)y * width * bpp;
        for (uint32_t x = 0; x < width; ++x) {
            const uint8_t *src = src_row + (size_t)x * bpp;
            uint8_t *dst = dst_row + (size_t)x * bpp;
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
        }
    }

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open output TGA: %s\n", path);
        free(pixels);
        return 0;
    }

    fputc(0, fp);
    fputc(0, fp);
    fputc(10, fp);
    write_le16(fp, 0);
    write_le16(fp, 0);
    fputc(0, fp);
    write_le16(fp, 0);
    write_le16(fp, 0);
    write_le16(fp, (uint16_t)width);
    write_le16(fp, (uint16_t)height);
    fputc(32, fp);
    fputc(8, fp);

    size_t i = 0;
    while (i < pixel_count) {
        size_t run = 1;
        while (i + run < pixel_count && run < 128 &&
               pixels_equal(pixels, i, i + run, bpp)) {
            run++;
        }

        if (run >= 2) {
            fputc((uint8_t)(0x80 | (run - 1)), fp);
            fwrite(pixels + i * bpp, bpp, 1, fp);
            i += run;
            continue;
        }

        size_t raw = 1;
        while (i + raw < pixel_count && raw < 128) {
            if (i + raw + 1 < pixel_count &&
                pixels_equal(pixels, i + raw, i + raw + 1, bpp)) {
                break;
            }
            raw++;
        }

        fputc((uint8_t)(raw - 1), fp);
        fwrite(pixels + i * bpp, bpp, raw, fp);
        i += raw;
    }

    free(pixels);
    fclose(fp);
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input.png> <output.tga>\n", argv[0]);
        return 1;
    }

    uint8_t *pixels = NULL;
    uint32_t width = 0;
    uint32_t height = 0;

    if (!load_png_rgba(argv[1], &pixels, &width, &height)) {
        return 1;
    }

    if (!write_tga_rle(argv[2], pixels, width, height)) {
        free(pixels);
        return 1;
    }

    free(pixels);
    return 0;
}
