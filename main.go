package main

import (
	"bufio"
	"fmt"
	"image"
	"image/draw"
	"image/png"
	"io"
	"os"
	"strings"
)

func writeLE16(w io.Writer, value uint16) error {
	bytes := [2]byte{byte(value), byte(value >> 8)}
	_, err := w.Write(bytes[:])
	return err
}

func loadPNGRGBA(path string) (*image.RGBA, error) {
	fp, err := os.Open(path)
	if err != nil {
		return nil, fmt.Errorf("failed to open input PNG: %s", path)
	}
	defer fp.Close()

	img, err := png.Decode(fp)
	if err != nil {
		return nil, fmt.Errorf("failed to decode PNG: %s", path)
	}

	bounds := img.Bounds()
	w := bounds.Dx()
	h := bounds.Dy()
	if w <= 0 || h <= 0 {
		return nil, fmt.Errorf("input PNG has invalid dimensions: %dx%d", w, h)
	}

	rgba := image.NewRGBA(image.Rect(0, 0, w, h))
	draw.Draw(rgba, rgba.Bounds(), img, bounds.Min, draw.Src)
	return rgba, nil
}

func pixelsEqual(pixels []byte, a, b, bpp int) bool {
	ai := a * bpp
	bi := b * bpp
	for i := 0; i < bpp; i++ {
		if pixels[ai+i] != pixels[bi+i] {
			return false
		}
	}
	return true
}

func makeBGRABottomLeft(rgba *image.RGBA) ([]byte, int, int) {
	w := rgba.Bounds().Dx()
	h := rgba.Bounds().Dy()
	bpp := 4
	pixels := make([]byte, w*h*bpp)

	for y := 0; y < h; y++ {
		srcY := h - 1 - y
		srcRow := srcY * rgba.Stride
		dstRow := y * w * bpp
		for x := 0; x < w; x++ {
			si := srcRow + x*bpp
			di := dstRow + x*bpp
			r := rgba.Pix[si]
			g := rgba.Pix[si+1]
			b := rgba.Pix[si+2]
			a := rgba.Pix[si+3]
			pixels[di] = b
			pixels[di+1] = g
			pixels[di+2] = r
			pixels[di+3] = a
		}
	}

	return pixels, w, h
}

func writeTGARLE(path string, rgba *image.RGBA) error {
	pixels, width, height := makeBGRABottomLeft(rgba)
	if width > 65535 || height > 65535 {
		return fmt.Errorf("TGA supports up to 65535x65535 pixels")
	}

	fp, err := os.Create(path)
	if err != nil {
		return fmt.Errorf("failed to open output TGA: %s", path)
	}
	defer fp.Close()

	writer := bufio.NewWriter(fp)
	defer writer.Flush()

	if err := writer.WriteByte(0); err != nil {
		return err
	}
	if err := writer.WriteByte(0); err != nil {
		return err
	}
	if err := writer.WriteByte(10); err != nil {
		return err
	}
	if err := writeLE16(writer, 0); err != nil {
		return err
	}
	if err := writeLE16(writer, 0); err != nil {
		return err
	}
	if err := writer.WriteByte(0); err != nil {
		return err
	}
	if err := writeLE16(writer, 0); err != nil {
		return err
	}
	if err := writeLE16(writer, 0); err != nil {
		return err
	}
	if err := writeLE16(writer, uint16(width)); err != nil {
		return err
	}
	if err := writeLE16(writer, uint16(height)); err != nil {
		return err
	}
	if err := writer.WriteByte(32); err != nil {
		return err
	}
	if err := writer.WriteByte(8); err != nil {
		return err
	}

	bpp := 4
	pixelCount := len(pixels) / bpp
	i := 0
	for i < pixelCount {
		run := 1
		for i+run < pixelCount && run < 128 && pixelsEqual(pixels, i, i+run, bpp) {
			run++
		}

		if run >= 2 {
			if err := writer.WriteByte(byte(0x80 | (run - 1))); err != nil {
				return err
			}
			if _, err := writer.Write(pixels[i*bpp : i*bpp+bpp]); err != nil {
				return err
			}
			i += run
			continue
		}

		raw := 1
		for i+raw < pixelCount && raw < 128 {
			if i+raw+1 < pixelCount && pixelsEqual(pixels, i+raw, i+raw+1, bpp) {
				break
			}
			raw++
		}

		if err := writer.WriteByte(byte(raw - 1)); err != nil {
			return err
		}
		if _, err := writer.Write(pixels[i*bpp : (i+raw)*bpp]); err != nil {
			return err
		}
		i += raw
	}

	return writer.Flush()
}

func main() {
	if len(os.Args) == 2 && (os.Args[1] == "-h" || os.Args[1] == "--help") {
		fmt.Fprintf(os.Stdout, "Usage: %s <input.png> [output.tga]\n", os.Args[0])
		fmt.Fprintln(os.Stdout, "Convert a PNG image to an idTech 3 compatible RLE TGA.")
		os.Exit(0)
	}
	if len(os.Args) != 2 && len(os.Args) != 3 {
		fmt.Fprintf(os.Stderr, "Usage: %s <input.png> [output.tga]\n", os.Args[0])
		fmt.Fprintf(os.Stderr, "Try '%s --help' for more information.\n", os.Args[0])
		os.Exit(1)
	}

	inputPath := os.Args[1]
	outputPath := ""
	if len(os.Args) == 3 {
		outputPath = os.Args[2]
	} else {
		if !strings.HasSuffix(inputPath, ".png") {
			fmt.Fprintln(os.Stderr, "input must end with .png when output is not provided")
			os.Exit(1)
		}
		outputPath = strings.TrimSuffix(inputPath, ".png") + ".tga"
	}

	rgba, err := loadPNGRGBA(inputPath)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}

	if err := writeTGARLE(outputPath, rgba); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}
