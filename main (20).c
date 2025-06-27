#include <stdio.h>
#include <stdlib.h> 
#include <strings.h>
#include <math.h> 
#include "lodepng.h" 
#define THRESHOLD 20
#define MIN_COMPONENT_SIZE 30

//в коде используются уже готовые функции и переменные из main.cpp - https://github.com/sheblaev/skull 

unsigned char* load_png(const char* filename, unsigned int* width, unsigned int* height) {
    unsigned char* image = NULL; 
    int error = lodepng_decode32_file(&image, width, height, filename);
    if(error != 0) {
        printf("error %u: %s\n", error, lodepng_error_text(error)); 
    }
    return image;
}

void write_png(const char* filename, const unsigned char* image, unsigned width, unsigned height) {
    unsigned char* png;
    size_t pngsize;
    int error = lodepng_encode32(&png, &pngsize, image, width, height);
    if(error == 0) {
        lodepng_save_file(png, pngsize, filename);
    } else { 
        printf("error %u: %s\n", error, lodepng_error_text(error));
    }
    free(png);
}

typedef struct {
    int* parent;
    int* rank;
} DSU;

DSU* dsu_create(int size) {
    DSU* dsu = (DSU*)malloc(sizeof(DSU));
    dsu->parent = (int*)malloc(size * sizeof(int));
    dsu->rank = (int*)malloc(size * sizeof(int));
    for (int i = 0; i < size; i++) {
        dsu->parent[i] = i;
        dsu->rank[i] = 0;
    }
    return dsu;
}

int dsu_find(DSU* dsu, int x) {
    if (dsu->parent[x] != x)
        dsu->parent[x] = dsu_find(dsu, dsu->parent[x]);
    return dsu->parent[x];
}

void dsu_union(DSU* dsu, int x, int y) {
    int xr = dsu_find(dsu, x);
    int yr = dsu_find(dsu, y);
    if (xr == yr) return;
    if (dsu->rank[xr] < dsu->rank[yr])
        dsu->parent[xr] = yr;
    else if (dsu->rank[xr] > dsu->rank[yr])
        dsu->parent[yr] = xr;
    else {
        dsu->parent[yr] = xr;
        dsu->rank[xr]++;
    }
}

void dsu_free(DSU* dsu) {
    free(dsu->parent);
    free(dsu->rank);
    free(dsu);
}

void rgb_to_grayscale(const unsigned char* rgba, unsigned char* gray, int size) {
    for (int i = 0; i < size; i++) {
        int r = rgba[4*i];
        int g = rgba[4*i+1];
        int b = rgba[4*i+2];
        gray[i] = (unsigned char)(0.299*r + 0.587*g + 0.114*b);
    }
}

void grayscale_to_rgba(const unsigned char* gray, unsigned char* rgba, int size) {
    for (int i = 0; i < size; i++) {
        rgba[i*4] = gray[i];
        rgba[i*4 + 1] = gray[i];
        rgba[i*4 + 2] = gray[i];
        rgba[i*4 + 3] = 255;
    }
}

void contrast(unsigned char* gray, int size) {
    for (int i = 0; i < size; i++) {
        if (gray[i] < 55) gray[i] = 0;
        else if (gray[i] > 195) gray[i] = 255;
    }
}

//используем медианный фильтр вместо гауссова размытия
unsigned char median(unsigned char* window, int size) {
    for (int i = 0; i < size - 1; i++)
        for (int j = i + 1; j < size; j++)
            if (window[j] < window[i]) {
                unsigned char t = window[i];
                window[i] = window[j];
                window[j] = t;
            }
    return window[size / 2];
}

void apply_median_filter(unsigned char* input, unsigned char* output, int width, int height) {
    int dx[] = {-1, 0, 1, -1, 0, 1, -1, 0, 1};
    int dy[] = {-1, -1, -1, 0, 0, 0, 1, 1, 1};
    unsigned char window[9];

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            for (int k = 0; k < 9; k++) {
                int nx = x + dx[k];
                int ny = y + dy[k];
                window[k] = input[ny * width + nx];
            }
            output[y * width + x] = median(window, 9);
        }
    }

    for (int x = 0; x < width; x++) {
        output[x] = input[x];
        output[(height - 1) * width + x] = input[(height - 1) * width + x];
    }
    for (int y = 0; y < height; y++) {
        output[y * width] = input[y * width];
        output[y * width + width - 1] = input[y * width + width - 1];
    }
}

// фильтр Лапласа
void apply_laplacian_filter(unsigned char* input, unsigned char* output, int width, int height) {
    int kernel[3][3] = {
        { 0, -1,  0},
        {-1,  4, -1},
        { 0, -1,  0}
    };

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int sum = 0;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int pixel = input[(y + ky) * width + (x + kx)];
                    sum += pixel * kernel[ky + 1][kx + 1];
                }
            }
            int idx = y * width + x;
            sum = abs(sum);
            if (sum > 255) sum = 255;
            output[idx] = (unsigned char)sum;
        }
    }
    // края без изменений
    for (int x = 0; x < width; x++) {
        output[x] = input[x];
        output[(height - 1) * width + x] = input[(height - 1) * width + x];
    }
    for (int y = 0; y < height; y++) {
        output[y * width] = input[y * width];
        output[y * width + width - 1] = input[y * width + width - 1];
    }
}



void segment(unsigned char* gray, unsigned char* out, int width, int height) {
    int size = width * height;
    DSU* dsu = dsu_create(size);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int curr = y * width + x;
            if (x < width - 1) {
                int right = curr + 1;
                if (abs(gray[curr] - gray[right]) < THRESHOLD)
                    dsu_union(dsu, curr, right);
            }
            if (y < height - 1) {
                int down = curr + width;
                if (abs(gray[curr] - gray[down]) < THRESHOLD)
                    dsu_union(dsu, curr, down);
            }
        }
    }

    int* sizes = (int*)calloc(size, sizeof(int));
    for (int i = 0; i < size; i++) {
        int root = dsu_find(dsu, i);
        sizes[root]++;
    }

    int* color_map = (int*)malloc(size * sizeof(int));
    int color_count = 0;
    for (int i = 0; i < size; i++) {
        if (sizes[i] >= MIN_COMPONENT_SIZE) {
            color_map[i] = color_count++;
        } else {
            color_map[i] = -1;
        }
    }

    int* colors = (int*)malloc(color_count * 3 * sizeof(int));
    srand(42);
    for (int i = 0; i < color_count; i++) {
        colors[i*3] = rand() % 256;
        colors[i*3+1] = rand() % 256;
        colors[i*3+2] = rand() % 256;
    }

    for (int i = 0; i < size; i++) {
        int root = dsu_find(dsu, i);
        int out_idx = i * 4;
        if (color_map[root] == -1) {
            out[out_idx] = out[out_idx+1] = out[out_idx+2] = 0;
        } else {
            int c = color_map[root];
            out[out_idx] = colors[c*3];
            out[out_idx+1] = colors[c*3+1];
            out[out_idx+2] = colors[c*3+2];
        }
        out[out_idx+3] = 255;
    }

    dsu_free(dsu);
    free(sizes);
    free(colors);
    free(color_map);
}

int main() {
    unsigned int width, height;
    unsigned char* image = load_png("skull.png", &width, &height);
    if (!image) return 1;

    int size = width * height;
    unsigned char* gray = (unsigned char*)malloc(size);
    unsigned char* temp_rgba = (unsigned char*)malloc(size * 4);
    unsigned char* smoothed = (unsigned char*)malloc(size);
    unsigned char* laplacian = (unsigned char*)malloc(size);
    unsigned char* result = (unsigned char*)malloc(size * 4);

//посмотрим на промежуточные картинки 

    rgb_to_grayscale(image, gray, size);
    grayscale_to_rgba(gray, temp_rgba, size);
    write_png("grayscale.png", temp_rgba, width, height); //после перевода в ЧБ

    contrast(gray, size);
    grayscale_to_rgba(gray, temp_rgba, size);
    write_png("contrast.png", temp_rgba, width, height); //после контрастирования

    apply_median_filter(gray, smoothed, width, height);
    grayscale_to_rgba(smoothed, temp_rgba, size);
    write_png("medianfilter.png", temp_rgba, width, height); //после медианного фильтра

    apply_laplacian_filter(smoothed, laplacian, width, height);
    grayscale_to_rgba(laplacian, temp_rgba, size);
    write_png("laplacian.png", temp_rgba, width, height); //после фильтра лапласа для границ
    
   segment(laplacian, result, width, height);
    write_png("final_seg.png", result, width, height); //финальный рез-т

    
    free(gray);
    free(smoothed);
    free(laplacian);
    free(result);
    free(temp_rgba);
    free(image);
    return 0;
}
