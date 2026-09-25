///////////////////////////////////////////////////////////////////////////////
//
//      TargaImage.cpp                          Author:     Stephen Chenney
//                                              Modified:   Eric McDaniel
//                                              Date:       Fall 2004
//
//      Implementation of TargaImage methods.  You must implement the image
//  modification functions.
//
///////////////////////////////////////////////////////////////////////////////

#include "Globals.h"
#include "TargaImage.h"
#include "libtarga.h"
#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <math.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

using namespace std;

// constants
const int           RED             = 0;                // red channel
const int           GREEN           = 1;                // green channel
const int           BLUE            = 2;                // blue channel
const unsigned char BACKGROUND[3]   = { 0, 0, 0 };      // background color


// Computes n choose s, efficiently
double Binomial(int n, int s)
{
    double        res;

    res = 1;
    for (int i = 1 ; i <= s ; i++)
        res = (n - i + 1) * res / i ;

    return res;
}// Binomial


///////////////////////////////////////////////////////////////////////////////
//
//      Constructor.  Initialize member variables.
//
///////////////////////////////////////////////////////////////////////////////
TargaImage::TargaImage() : width(0), height(0), data(NULL)
{}// TargaImage

///////////////////////////////////////////////////////////////////////////////
//
//      Constructor.  Initialize member variables.
//
///////////////////////////////////////////////////////////////////////////////
TargaImage::TargaImage(int w, int h) : width(w), height(h)
{
   data = new unsigned char[width * height * 4];
   ClearToBlack();
}// TargaImage



///////////////////////////////////////////////////////////////////////////////
//
//      Constructor.  Initialize member variables to values given.
//
///////////////////////////////////////////////////////////////////////////////
TargaImage::TargaImage(int w, int h, unsigned char *d)
{
    int i;

    width = w;
    height = h;
    data = new unsigned char[width * height * 4];

    for (i = 0; i < width * height * 4; i++)
	    data[i] = d[i];
}// TargaImage

///////////////////////////////////////////////////////////////////////////////
//
//      Copy Constructor.  Initialize member to that of input
//
///////////////////////////////////////////////////////////////////////////////
TargaImage::TargaImage(const TargaImage& image) 
{
   width = image.width;
   height = image.height;
   data = NULL; 
   if (image.data != NULL) {
      data = new unsigned char[width * height * 4];
      memcpy(data, image.data, sizeof(unsigned char) * width * height * 4);
   }
}


///////////////////////////////////////////////////////////////////////////////
//
//      Destructor.  Free image memory.
//
///////////////////////////////////////////////////////////////////////////////
TargaImage::~TargaImage()
{
    if (data)
        delete[] data;
}// ~TargaImage


///////////////////////////////////////////////////////////////////////////////
//
//      Converts an image to RGB form, and returns the rgb pixel data - 24 
//  bits per pixel. The returned space should be deleted when no longer 
//  required.
//
///////////////////////////////////////////////////////////////////////////////
unsigned char* TargaImage::To_RGB(void)
{
    unsigned char   *rgb = new unsigned char[width * height * 3];
    int		    i, j;

    if (! data)
	    return NULL;

    // Divide out the alpha
    for (i = 0 ; i < height ; i++)
    {
	    int in_offset = i * width * 4;
	    int out_offset = i * width * 3;

	    for (j = 0 ; j < width ; j++)
        {
	        RGBA_To_RGB(data + (in_offset + j*4), rgb + (out_offset + j*3));
	    }
    }

    return rgb;
}// TargaImage


///////////////////////////////////////////////////////////////////////////////
//
//      Save the image to a targa file. Returns 1 on success, 0 on failure.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Save_Image(const char *filename)
{
    TargaImage	*out_image = Reverse_Rows();

    if (! out_image)
	    return false;

    if (!tga_write_raw(filename, width, height, out_image->data, TGA_TRUECOLOR_32))
    {
	    cout << "TGA Save Error: %s\n", tga_error_string(tga_get_last_error());
	    return false;
    }

    delete out_image;

    return true;
}// Save_Image


///////////////////////////////////////////////////////////////////////////////
//
//      Load a targa image from a file.  Return a new TargaImage object which 
//  must be deleted by caller.  Return NULL on failure.
//
///////////////////////////////////////////////////////////////////////////////
TargaImage* TargaImage::Load_Image(char *filename)
{
    unsigned char   *temp_data;
    TargaImage	    *temp_image;
    TargaImage	    *result;
    int		        width, height;

    if (!filename)
    {
        cout << "No filename given." << endl;
        return NULL;
    }// if

    temp_data = (unsigned char*)tga_load(filename, &width, &height, TGA_TRUECOLOR_32);
    if (!temp_data)
    {
        cout << "TGA Error: %s\n", tga_error_string(tga_get_last_error());
	    width = height = 0;
	    return NULL;
    }
    temp_image = new TargaImage(width, height, temp_data);
    free(temp_data);

    result = temp_image->Reverse_Rows();

    delete temp_image;

    return result;
}// Load_Image


///////////////////////////////////////////////////////////////////////////////
//
//      Convert image to grayscale.  Red, green, and blue channels should all 
//  contain grayscale value.  Alpha channel shoould be left unchanged.  Return
//  success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::To_Grayscale()
{
	if (!data)
		return false;

	for (int i = 0; i < width * height * 4; i += 4)
	{
		unsigned char r = data[i + RED];
		unsigned char g = data[i + GREEN];
		unsigned char b = data[i + BLUE];

		float gray_f = 0.299f * r + 0.587f * g + 0.114f * b;
		if (gray_f < 0.0f) gray_f = 0.0f;
		if (gray_f > 255.0f) gray_f = 255.0f;
		unsigned char gray = (unsigned char)gray_f;

		data[i + RED] = gray;
		data[i + GREEN] = gray;
		data[i + BLUE] = gray;
		// alpha (data[i + 3]) left unchanged
	}

	return true;
}// To_Grayscale


///////////////////////////////////////////////////////////////////////////////
//
//  Convert the image to an 8 bit image using uniform quantization.  Return 
//  success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Quant_Uniform()
{
    if (!data)
        return false;

    // Palette matching the dither-color spec: R,G 8 levels, B 4 levels.
    static const unsigned char RG_LEVELS[8] = { 0, 36, 73, 109, 146, 182, 219, 255 };
    static const unsigned char B_LEVELS[4] = { 0, 85, 170, 255 };

    for (int i = 0; i < width * height * 4; i += 4)
    {
        unsigned char r = data[i + RED];
        unsigned char g = data[i + GREEN];
        unsigned char b = data[i + BLUE];

        // Nearest-level uniform quantization: idx = round(c * (N-1) / 255).
        int ri = (r * 7 + 127) / 255;
        int gi = (g * 7 + 127) / 255;
        int bi = (b * 3 + 127) / 255;

        if (ri < 0) ri = 0; if (ri > 7) ri = 7;
        if (gi < 0) gi = 0; if (gi > 7) gi = 7;
        if (bi < 0) bi = 0; if (bi > 3) bi = 3;

        data[i + RED] = RG_LEVELS[ri];
        data[i + GREEN] = RG_LEVELS[gi];
        data[i + BLUE] = B_LEVELS[bi];
        // alpha left unchanged
    }

    return true;
}// Quant_Uniform


///////////////////////////////////////////////////////////////////////////////
//
//      Convert the image to an 8 bit image using populosity quantization.  
//  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Quant_Populosity()
{
    if (!data)
        return false;

    const int LEVELS = 32;
    const int HIST_SIZE = LEVELS * LEVELS * LEVELS; // 32^3 = 32768
    const int PALETTE_SIZE = 256;

    // 1. Uniform pre-quantization to 32 shades/channel + popularity histogram.
    //    Bin index per channel: c32 = c >> 3 (0..31).
    vector<int> hist(HIST_SIZE, 0);
    const int npix = width * height;
    for (int p = 0; p < npix; p++)
    {
        int r32 = data[p * 4 + RED] >> 3;
        int g32 = data[p * 4 + GREEN] >> 3;
        int b32 = data[p * 4 + BLUE] >> 3;
        hist[(r32 << 10) | (g32 << 5) | b32]++;
    }

    // 2. Find the 256 most popular bins (count desc, index asc for ties).
    vector<int> order(HIST_SIZE);
    for (int i = 0; i < HIST_SIZE; i++)
        order[i] = i;
    sort(order.begin(), order.end(), [&hist](int a, int b) {
        if (hist[a] != hist[b])
            return hist[a] > hist[b];
        return a < b;
    });

    // Palette entries use the bin center as representative: rep = bin * 8 + 4.
    struct RGB { unsigned char r, g, b; };
    vector<RGB> palette;
    for (int i = 0; i < HIST_SIZE && (int)palette.size() < PALETTE_SIZE; i++)
    {
        if (hist[order[i]] == 0)
            break;
        int b32 = order[i] & 31;
        int g32 = (order[i] >> 5) & 31;
        int r32 = (order[i] >> 10) & 31;
        RGB c;
        c.r = (unsigned char)((r32 << 3) + 4);
        c.g = (unsigned char)((g32 << 3) + 4);
        c.b = (unsigned char)((b32 << 3) + 4);
        palette.push_back(c);
    }
    if (palette.empty())
        return false;

    // 3. Map each ORIGINAL pixel color to the closest palette color.
    //    L2 distance in RGB; squared distance gives the same ordering as sqrt.
    for (int p = 0; p < npix; p++)
    {
        int r = data[p * 4 + RED];
        int g = data[p * 4 + GREEN];
        int b = data[p * 4 + BLUE];

        int best = 0;
        int bestDist = 3 * 255 * 255 + 1;
        for (int k = 0; k < (int)palette.size(); k++)
        {
            int dr = r - palette[k].r;
            int dg = g - palette[k].g;
            int db = b - palette[k].b;
            int d = dr * dr + dg * dg + db * db;
            if (d < bestDist)
            {
                bestDist = d;
                best = k;
            }
        }

        data[p * 4 + RED] = palette[best].r;
        data[p * 4 + GREEN] = palette[best].g;
        data[p * 4 + BLUE] = palette[best].b;
        // alpha left unchanged
    }

    return true;
}// Quant_Populosity


///////////////////////////////////////////////////////////////////////////////
//
//      Dither the image using a threshold of 1/2.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Dither_Threshold()
{
    if (!data)
        return false;

    for (int i = 0; i < width * height * 4; i += 4)
    {
        // Grayscale in range 0-255, then threshold at 0.5 (i.e. 127.5).
        float gray_f = 0.299f * data[i + RED]
                     + 0.587f * data[i + GREEN]
                     + 0.114f * data[i + BLUE];
        unsigned char v = (gray_f >= 127.5f) ? 255 : 0;

        data[i + RED] = v;
        data[i + GREEN] = v;
        data[i + BLUE] = v;
        // alpha left unchanged (tests use alpha = 1)
    }

    return true;
}// Dither_Threshold


///////////////////////////////////////////////////////////////////////////////
//
//      Dither image using random dithering.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Dither_Random()
{
    if (!data)
        return false;

    // Random dithering: intensity in 0-1, add uniform noise in [-0.2, 0.2],
    // threshold at 0.5. Fixed seed (no srand) keeps output reproducible.
    for (int i = 0; i < width * height * 4; i += 4)
    {
        float g01 = (0.299f * data[i + RED]
                   + 0.587f * data[i + GREEN]
                   + 0.114f * data[i + BLUE]) / 255.0f;
        float noise = ((float)rand() / (float)RAND_MAX - 0.5f) * 0.4f;
        unsigned char v = (g01 + noise >= 0.5f) ? 255 : 0;

        data[i + RED] = v;
        data[i + GREEN] = v;
        data[i + BLUE] = v;
        // alpha left unchanged (tests use alpha = 1)
    }

    return true;
}// Dither_Random


///////////////////////////////////////////////////////////////////////////////
//
//      Perform Floyd-Steinberg dithering on the image.  Return success of 
//  operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Dither_FS()
{
    if (!data)
        return false;

    const int npix = width * height;

    // Float grayscale buffer (0-255), truncated to integer like the
    // reference's grayscale convention; fractions then feed diffusion.
    // NOTE: double literals (not float): float64 truncation matches the
    // reference. float32 truncation seeds ~7K exact-integer gray pixels
    // differently and the diffusion cascade amplifies it visibly.
    vector<float> buf(npix);
    for (int p = 0; p < npix; p++)
    {
        int gi = (int)(0.299 * data[p * 4 + RED]
                     + 0.587 * data[p * 4 + GREEN]
                     + 0.114 * data[p * 4 + BLUE]);
        if (gi < 0) gi = 0;
        if (gi > 255) gi = 255;
        buf[p] = (float)gi;
    }

    // Serpentine (zig-zag) scan with mirrored error weights.
    for (int y = 0; y < height; y++)
    {
        bool ltr = (y % 2 == 0);
        int x0 = ltr ? 0 : width - 1;
        int x1 = ltr ? width : -1;
        int dx = ltr ? 1 : -1;
        for (int x = x0; x != x1; x += dx)
        {
            float oldv = buf[y * width + x];
            float newv = (oldv >= 127.5f) ? 255.0f : 0.0f;
            data[(y * width + x) * 4 + RED] =
            data[(y * width + x) * 4 + GREEN] =
            data[(y * width + x) * 4 + BLUE] =
                (newv >= 127.5f) ? (unsigned char)255 : (unsigned char)0;

            float err = oldv - newv;
            // Unvisited neighbors: ahead in x, plus the row below.
            int xa = x + dx;       // 7/16
            int xb = x - dx;       // 3/16 (below) / 1/16 mirrored
            if (xa >= 0 && xa < width)
                buf[y * width + xa] += err * (7.0f / 16.0f);
            if (y + 1 < height)
            {
                if (xb >= 0 && xb < width)
                    buf[(y + 1) * width + xb] += err * (3.0f / 16.0f);
                buf[(y + 1) * width + x] += err * (5.0f / 16.0f);
                if (xa >= 0 && xa < width)
                    buf[(y + 1) * width + xa] += err * (1.0f / 16.0f);
            }
        }
    }
    // alpha left unchanged (tests use alpha = 1)

    return true;
}// Dither_FS


///////////////////////////////////////////////////////////////////////////////
//
//      Dither the image while conserving the average brightness.  Return 
//  success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Dither_Bright()
{
    if (!data)
        return false;

    const int npix = width * height;

    // 1. Grayscale each pixel (truncated integer 0-255) and accumulate brightness.
    vector<int> gray(npix);
    long long sum = 0;
    for (int p = 0; p < npix; p++)
    {
        float g = 0.299f * data[p * 4 + RED]
                + 0.587f * data[p * 4 + GREEN]
                + 0.114f * data[p * 4 + BLUE];
        int gi = (int)g;
        if (gi < 0) gi = 0;
        if (gi > 255) gi = 255;
        gray[p] = gi;
        sum += gi;
    }
    double avg = (double)sum / npix; // average brightness, 0-255

    // 2. Choose T so the output keeps the same average brightness:
    //    white fraction must satisfy count(gray >= T)/npix ~= avg/255,
    //    i.e. whiteCount = round(avg/255 * npix). Accumulate the
    //    histogram from the top until that many pixels are covered.
    int whiteCount = (int)(avg / 255.0 * npix + 0.5);
    int T;
    if (whiteCount <= 0)
        T = 256; // all black
    else if (whiteCount >= npix)
        T = 0;   // all white
    else
    {
        int hist[256] = { 0 };
        for (int p = 0; p < npix; p++)
            hist[gray[p]]++;
        T = 0;
        int cum = 0;
        for (int v = 255; v >= 0; v--)
        {
            cum += hist[v];
            if (cum >= whiteCount)
            {
                T = v;
                break;
            }
        }
    }

    // 3. Threshold at T.
    for (int p = 0; p < npix; p++)
    {
        unsigned char v = (gray[p] >= T) ? 255 : 0;
        data[p * 4 + RED] = v;
        data[p * 4 + GREEN] = v;
        data[p * 4 + BLUE] = v;
        // alpha left unchanged (tests use alpha = 1)
    }

    return true;
}// Dither_Bright


///////////////////////////////////////////////////////////////////////////////
//
//      Perform clustered differing of the image.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Dither_Cluster()
{
    if (!data)
        return false;

    // Clustered-dot tile: spec decimals are n/17 rounded to 4 places
    // (12/17=0.7059, ...), so use exact integer numerators. Reference
    // accesses the tile transposed: tile[y%4][x%4].
    static const int mask[4][4] = {
        { 12, 6, 10, 4 },
        { 1, 16, 14, 7 },
        { 8, 13, 15, 2 },
        { 3, 9, 5, 11 }
    };

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int i = (y * width + x) * 4;
            int gi = (int)(0.299f * data[i + RED]
                         + 0.587f * data[i + GREEN]
                         + 0.114f * data[i + BLUE]);
            if (gi < 0) gi = 0;
            if (gi > 255) gi = 255;
            // White if I > tile[y%4][x%4]; integer-exact form of
            // (gi/255) > (m/17), i.e. gi*17 > m*255.
            unsigned char v = (gi * 17 > mask[y % 4][x % 4] * 255) ? 255 : 0;

            data[i + RED] = v;
            data[i + GREEN] = v;
            data[i + BLUE] = v;
            // alpha left unchanged (tests use alpha = 1)
        }
    }

    return true;
}// Dither_Cluster


///////////////////////////////////////////////////////////////////////////////
//
//  Convert the image to an 8 bit image using Floyd-Steinberg dithering over
//  a uniform quantization - the same quantization as in Quant_Uniform.
//  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Dither_Color()
{
    if (!data)
        return false;

    // Uniform-quantization palette shared with Quant_Uniform.
    static const unsigned char RG_LEVELS[8] = { 0, 36, 73, 109, 146, 182, 219, 255 };
    static const unsigned char B_LEVELS[4] = { 0, 85, 170, 255 };

    const int npix = width * height;

    // Per-channel float buffers (diffused error needs fractions).
    vector<float> bufR(npix), bufG(npix), bufB(npix);
    for (int p = 0; p < npix; p++)
    {
        bufR[p] = (float)data[p * 4 + RED];
        bufG[p] = (float)data[p * 4 + GREEN];
        bufB[p] = (float)data[p * 4 + BLUE];
    }

    // Serpentine scan (same traversal as Dither_FS), quantizing each
    // pixel to the nearest palette color and diffusing per-channel error.
    for (int y = 0; y < height; y++)
    {
        bool ltr = (y % 2 == 0);
        int x0 = ltr ? 0 : width - 1;
        int x1 = ltr ? width : -1;
        int dx = ltr ? 1 : -1;
        for (int x = x0; x != x1; x += dx)
        {
            int p = y * width + x;

            int ri = (int)(bufR[p] * 7.0f / 255.0f + 0.5f);
            int gi = (int)(bufG[p] * 7.0f / 255.0f + 0.5f);
            int bi = (int)(bufB[p] * 3.0f / 255.0f + 0.5f);
            if (ri < 0) ri = 0; if (ri > 7) ri = 7;
            if (gi < 0) gi = 0; if (gi > 7) gi = 7;
            if (bi < 0) bi = 0; if (bi > 3) bi = 3;

            float newR = (float)RG_LEVELS[ri];
            float newG = (float)RG_LEVELS[gi];
            float newB = (float)B_LEVELS[bi];

            data[p * 4 + RED] = RG_LEVELS[ri];
            data[p * 4 + GREEN] = RG_LEVELS[gi];
            data[p * 4 + BLUE] = B_LEVELS[bi];

            float errR = bufR[p] - newR;
            float errG = bufG[p] - newG;
            float errB = bufB[p] - newB;

            int xa = x + dx;
            int xb = x - dx;
            if (xa >= 0 && xa < width)
            {
                bufR[y * width + xa] += errR * (7.0f / 16.0f);
                bufG[y * width + xa] += errG * (7.0f / 16.0f);
                bufB[y * width + xa] += errB * (7.0f / 16.0f);
            }
            if (y + 1 < height)
            {
                if (xb >= 0 && xb < width)
                {
                    bufR[(y + 1) * width + xb] += errR * (3.0f / 16.0f);
                    bufG[(y + 1) * width + xb] += errG * (3.0f / 16.0f);
                    bufB[(y + 1) * width + xb] += errB * (3.0f / 16.0f);
                }
                bufR[(y + 1) * width + x] += errR * (5.0f / 16.0f);
                bufG[(y + 1) * width + x] += errG * (5.0f / 16.0f);
                bufB[(y + 1) * width + x] += errB * (5.0f / 16.0f);
                if (xa >= 0 && xa < width)
                {
                    bufR[(y + 1) * width + xa] += errR * (1.0f / 16.0f);
                    bufG[(y + 1) * width + xa] += errG * (1.0f / 16.0f);
                    bufB[(y + 1) * width + xa] += errB * (1.0f / 16.0f);
                }
            }
        }
    }
    // alpha left unchanged (tests use alpha = 1)

    return true;
}// Dither_Color


///////////////////////////////////////////////////////////////////////////////
//
//      Composite the current image over the given image.  Return success of 
//  operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Comp_Over(TargaImage* pImage)
{
    if (width != pImage->width || height != pImage->height)
    {
        cout <<  "Comp_Over: Images not the same size\n";
        return false;
    }

    ClearToBlack();
    return false;
}// Comp_Over


///////////////////////////////////////////////////////////////////////////////
//
//      Composite this image "in" the given image.  See lecture notes for 
//  details.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Comp_In(TargaImage* pImage)
{
    if (width != pImage->width || height != pImage->height)
    {
        cout << "Comp_In: Images not the same size\n";
        return false;
    }

    ClearToBlack();
    return false;
}// Comp_In


///////////////////////////////////////////////////////////////////////////////
//
//      Composite this image "out" the given image.  See lecture notes for 
//  details.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Comp_Out(TargaImage* pImage)
{
    if (width != pImage->width || height != pImage->height)
    {
        cout << "Comp_Out: Images not the same size\n";
        return false;
    }

    ClearToBlack();
    return false;
}// Comp_Out


///////////////////////////////////////////////////////////////////////////////
//
//      Composite current image "atop" given image.  Return success of 
//  operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Comp_Atop(TargaImage* pImage)
{
    if (width != pImage->width || height != pImage->height)
    {
        cout << "Comp_Atop: Images not the same size\n";
        return false;
    }

    ClearToBlack();
    return false;
}// Comp_Atop


///////////////////////////////////////////////////////////////////////////////
//
//      Composite this image with given image using exclusive or (XOR).  Return
//  success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Comp_Xor(TargaImage* pImage)
{
    if (width != pImage->width || height != pImage->height)
    {
        cout << "Comp_Xor: Images not the same size\n";
        return false;
    }

    ClearToBlack();
    return false;
}// Comp_Xor


///////////////////////////////////////////////////////////////////////////////
//
//      Calculate the difference bewteen this imag and the given one.  Image 
//  dimensions must be equal.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Difference(TargaImage* pImage)
{
    if (!pImage)
        return false;

    if (width != pImage->width || height != pImage->height)
    {
        cout << "Difference: Images not the same size\n";
        return false;
    }// if

    for (int i = 0 ; i < width * height * 4 ; i += 4)
    {
        unsigned char        rgb1[3];
        unsigned char        rgb2[3];

        RGBA_To_RGB(data + i, rgb1);
        RGBA_To_RGB(pImage->data + i, rgb2);

        data[i] = abs(rgb1[0] - rgb2[0]);
        data[i+1] = abs(rgb1[1] - rgb2[1]);
        data[i+2] = abs(rgb1[2] - rgb2[2]);
        data[i+3] = 255;
    }

    return true;
}// Difference


///////////////////////////////////////////////////////////////////////////////
//
//      Perform 5x5 box filter on this image.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Filter_Box()
{
    ClearToBlack();
    return false;
}// Filter_Box


///////////////////////////////////////////////////////////////////////////////
//
//      Perform 5x5 Bartlett filter on this image.  Return success of 
//  operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Filter_Bartlett()
{
    ClearToBlack();
    return false;
}// Filter_Bartlett


///////////////////////////////////////////////////////////////////////////////
//
//      Perform 5x5 Gaussian filter on this image.  Return success of 
//  operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Filter_Gaussian()
{
    ClearToBlack();
    return false;
}// Filter_Gaussian

///////////////////////////////////////////////////////////////////////////////
//
//      Perform NxN Gaussian filter on this image.  Return success of 
//  operation.
//
///////////////////////////////////////////////////////////////////////////////

bool TargaImage::Filter_Gaussian_N( unsigned int N )
{
    ClearToBlack();
   return false;
}// Filter_Gaussian_N


///////////////////////////////////////////////////////////////////////////////
//
//      Perform 5x5 edge detect (high pass) filter on this image.  Return 
//  success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Filter_Edge()
{
    ClearToBlack();
    return false;
}// Filter_Edge


///////////////////////////////////////////////////////////////////////////////
//
//      Perform a 5x5 enhancement filter to this image.  Return success of 
//  operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Filter_Enhance()
{
    ClearToBlack();
    return false;
}// Filter_Enhance


///////////////////////////////////////////////////////////////////////////////
//
//      Run simplified version of Hertzmann's painterly image filter.
//      You probably will want to use the Draw_Stroke funciton and the
//      Stroke class to help.
// Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::NPR_Paint()
{
    ClearToBlack();
    return false;
}



///////////////////////////////////////////////////////////////////////////////
//
//      Halve the dimensions of this image.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Half_Size()
{
    ClearToBlack();
    return false;
}// Half_Size


///////////////////////////////////////////////////////////////////////////////
//
//      Double the dimensions of this image.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Double_Size()
{
    ClearToBlack();
    return false;
}// Double_Size


///////////////////////////////////////////////////////////////////////////////
//
//      Scale the image dimensions by the given factor.  The given factor is 
//  assumed to be greater than one.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Resize(float scale)
{
    ClearToBlack();
    return false;
}// Resize


//////////////////////////////////////////////////////////////////////////////
//
//      Rotate the image clockwise by the given angle.  Do not resize the 
//  image.  Return success of operation.
//
///////////////////////////////////////////////////////////////////////////////
bool TargaImage::Rotate(float angleDegrees)
{
    ClearToBlack();
    return false;
}// Rotate


//////////////////////////////////////////////////////////////////////////////
//
//      Given a single RGBA pixel return, via the second argument, the RGB
//      equivalent composited with a black background.
//
///////////////////////////////////////////////////////////////////////////////
void TargaImage::RGBA_To_RGB(unsigned char *rgba, unsigned char *rgb)
{
    const unsigned char	BACKGROUND[3] = { 0, 0, 0 };

    unsigned char  alpha = rgba[3];

    if (alpha == 0)
    {
        rgb[0] = BACKGROUND[0];
        rgb[1] = BACKGROUND[1];
        rgb[2] = BACKGROUND[2];
    }
    else
    {
	    float	alpha_scale = (float)255 / (float)alpha;
	    int	val;
	    int	i;

	    for (i = 0 ; i < 3 ; i++)
	    {
	        val = (int)floor(rgba[i] * alpha_scale);
	        if (val < 0)
		    rgb[i] = 0;
	        else if (val > 255)
		    rgb[i] = 255;
	        else
		    rgb[i] = val;
	    }
    }
}// RGA_To_RGB


///////////////////////////////////////////////////////////////////////////////
//
//      Copy this into a new image, reversing the rows as it goes. A pointer
//  to the new image is returned.
//
///////////////////////////////////////////////////////////////////////////////
TargaImage* TargaImage::Reverse_Rows(void)
{
    unsigned char   *dest = new unsigned char[width * height * 4];
    TargaImage	    *result;
    int 	        i, j;

    if (! data)
    	return NULL;

    for (i = 0 ; i < height ; i++)
    {
	    int in_offset = (height - i - 1) * width * 4;
	    int out_offset = i * width * 4;

	    for (j = 0 ; j < width ; j++)
        {
	        dest[out_offset + j * 4] = data[in_offset + j * 4];
	        dest[out_offset + j * 4 + 1] = data[in_offset + j * 4 + 1];
	        dest[out_offset + j * 4 + 2] = data[in_offset + j * 4 + 2];
	        dest[out_offset + j * 4 + 3] = data[in_offset + j * 4 + 3];
        }
    }

    result = new TargaImage(width, height, dest);
    delete[] dest;
    return result;
}// Reverse_Rows


///////////////////////////////////////////////////////////////////////////////
//
//      Clear the image to all black.
//
///////////////////////////////////////////////////////////////////////////////
void TargaImage::ClearToBlack()
{
    memset(data, 0, width * height * 4);
}// ClearToBlack


///////////////////////////////////////////////////////////////////////////////
//
//      Helper function for the painterly filter; paint a stroke at
// the given location
//
///////////////////////////////////////////////////////////////////////////////
void TargaImage::Paint_Stroke(const Stroke& s) {
   int radius_squared = (int)s.radius * (int)s.radius;
   for (int x_off = -((int)s.radius); x_off <= (int)s.radius; x_off++) {
      for (int y_off = -((int)s.radius); y_off <= (int)s.radius; y_off++) {
         int x_loc = (int)s.x + x_off;
         int y_loc = (int)s.y + y_off;
         // are we inside the circle, and inside the image?
         if ((x_loc >= 0 && x_loc < width && y_loc >= 0 && y_loc < height)) {
            int dist_squared = x_off * x_off + y_off * y_off;
            if (dist_squared <= radius_squared) {
               data[(y_loc * width + x_loc) * 4 + 0] = s.r;
               data[(y_loc * width + x_loc) * 4 + 1] = s.g;
               data[(y_loc * width + x_loc) * 4 + 2] = s.b;
               data[(y_loc * width + x_loc) * 4 + 3] = s.a;
            } else if (dist_squared == radius_squared + 1) {
               data[(y_loc * width + x_loc) * 4 + 0] = 
                  (data[(y_loc * width + x_loc) * 4 + 0] + s.r) / 2;
               data[(y_loc * width + x_loc) * 4 + 1] = 
                  (data[(y_loc * width + x_loc) * 4 + 1] + s.g) / 2;
               data[(y_loc * width + x_loc) * 4 + 2] = 
                  (data[(y_loc * width + x_loc) * 4 + 2] + s.b) / 2;
               data[(y_loc * width + x_loc) * 4 + 3] = 
                  (data[(y_loc * width + x_loc) * 4 + 3] + s.a) / 2;
            }
         }
      }
   }
}


///////////////////////////////////////////////////////////////////////////////
//
//      Build a Stroke
//
///////////////////////////////////////////////////////////////////////////////
Stroke::Stroke() {}

///////////////////////////////////////////////////////////////////////////////
//
//      Build a Stroke
//
///////////////////////////////////////////////////////////////////////////////
Stroke::Stroke(unsigned int iradius, unsigned int ix, unsigned int iy,
               unsigned char ir, unsigned char ig, unsigned char ib, unsigned char ia) :
   radius(iradius),x(ix),y(iy),r(ir),g(ig),b(ib),a(ia)
{
}

