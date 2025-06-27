#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "lodepng.h"

#define THRESHOLD 30
#define MIN_COMPONENT_SIZE 100
#define BACKGROUND_THRESHOLD 50

//в коде используются уже готовые функции и переменные из main.cpp - https://github.com/sheblaev/skull 

typedef struct {
    int* parent;
    int* rank;
} DSU;


unsigned char* load_png(const char* filename, unsigned* width, unsigned* height) {
    unsigned char* image = NULL;
    unsigned error = lodepng_decode32_file(&image, width, height, filename);
    if (error) {
        printf("Error %u: %s\n", error, lodepng_error_text(error));
        return NULL;
    }
    return image;
}

void save_png(const char* filename, const unsigned char* image, unsigned width, unsigned height) {
    unsigned error = lodepng_encode32_file(filename, image, width, height);
    if (error) {
        printf("Error %u: %s\n", error, lodepng_error_text(error));
    }
}

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
    if (dsu->parent[x] != x) {
        dsu->parent[x] = dsu_find(dsu, dsu->parent[x]);
    }
    return dsu->parent[x];
}

void dsu_union(DSU* dsu, int x, int y) {
    int x_root = dsu_find(dsu, x);
    int y_root = dsu_find(dsu, y);
    
    if (x_root == y_root) return;
    
    if (dsu->rank[x_root] < dsu->rank[y_root]) {
        dsu->parent[x_root] = y_root;
    } else {
        dsu->parent[y_root] = x_root;
        if (dsu->rank[x_root] == dsu->rank[y_root]) {
            dsu->rank[x_root]++;
        }
    }
}

void dsu_free(DSU* dsu) {
    free(dsu->parent);
    free(dsu->rank);
    free(dsu);
}

void rgb_to_grayscale(const unsigned char* rgba, unsigned char* gray, int size) {
    for (int i = 0; i < size; i++) {
        unsigned char r = rgba[4*i];
        unsigned char g = rgba[4*i+1];
        unsigned char b = rgba[4*i+2];
        gray[i] = (unsigned char)(0.299*r + 0.587*g + 0.114*b);
    }
}

void enhance_contrast(unsigned char* gray, int size) {
    for (int i = 0; i < size; i++) {
        if (gray[i] < BACKGROUND_THRESHOLD) {
            gray[i] = 0;
        } else if (gray[i] > 255 - BACKGROUND_THRESHOLD) {
            gray[i] = 255;
        }
    }
}

//применяем медианный фильтр вместо гауссова размытия
void median_filter(const unsigned char* input, unsigned char* output, int width, int height) {
    for (int y = 1; y < height-1; y++) {
        for (int x = 1; x < width-1; x++) {
            unsigned char window[9];
            int k = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    window[k++] = input[(y+dy)*width + (x+dx)];
                }
            }
            
            for (int i = 0; i < 5; i++) {
                for (int j = i+1; j < 9; j++) {
                    if (window[j] < window[i]) {
                        unsigned char temp = window[i];
                        window[i] = window[j];
                        window[j] = temp;
                    }
                }
            }
            output[y*width + x] = window[4];
        }
    }
}

void segment_image(const unsigned char* gray, unsigned char* output, int width, int height) {
    int size = width * height;
    DSU* dsu = dsu_create(size);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int current = y * width + x;
            
            if (x < width-1) {  
                int right = current + 1;
                if (abs(gray[current] - gray[right]) < THRESHOLD) {
                    dsu_union(dsu, current, right);
                }
            }
            
            if (y < height-1) {  
                int bottom = current + width;
                if (abs(gray[current] - gray[bottom]) < THRESHOLD) {
                    dsu_union(dsu, current, bottom);
                }
            }
        }
    }
    
    int* component_sizes = (int*)calloc(size, sizeof(int));
    for (int i = 0; i < size; i++) {
        int root = dsu_find(dsu, i);
        component_sizes[root]++;
    }
    

    srand(time(NULL));
    int* component_colors = (int*)malloc(size * 3 * sizeof(int));
    int color_index = 0;
    
    for (int i = 0; i < size; i++) {
        if (component_sizes[i] >= MIN_COMPONENT_SIZE) {
            component_colors[3*i] = 50 + rand() % 206;   // R
            component_colors[3*i+1] = 50 + rand() % 206; // G
            component_colors[3*i+2] = 50 + rand() % 206; // B
            color_index++;
        }
    }

    for (int i = 0; i < size; i++) {
        int root = dsu_find(dsu, i);
        int out_idx = 4*i;
        
        if (component_sizes[root] < MIN_COMPONENT_SIZE || gray[i] < BACKGROUND_THRESHOLD) {

            output[out_idx] = output[out_idx+1] = output[out_idx+2] = 0;
        } else {

            output[out_idx] = component_colors[3*root];
            output[out_idx+1] = component_colors[3*root+1];
            output[out_idx+2] = component_colors[3*root+2];
        }
        output[out_idx+3] = 255; 
    }
    

    free(component_sizes);
    free(component_colors);
    dsu_free(dsu);
}

int main() {
    const char* input_filename = "input.png";
    const char* output_filename = "output.png";
    
    unsigned width, height;
    unsigned char* image = load_png(input_filename, &width, &height);
    if (!image) {
        printf("failed to load image\n");
        return 1;
    }
    
    int size = width * height;
    unsigned char* gray_image = (unsigned char*)malloc(size);
    unsigned char* filtered_image = (unsigned char*)malloc(size);
    unsigned char* output_image = (unsigned char*)malloc(size * 4);
    
    if (!gray_image || !filtered_image || !output_image) {
        printf("memory allocation failed\n");
        return 1;
    }

    rgb_to_grayscale(image, gray_image, size);
    enhance_contrast(gray_image, size);
    median_filter(gray_image, filtered_image, width, height);
    segment_image(filtered_image, output_image, width, height);
    
    save_png(output_filename, output_image, width, height);
    printf("segmentation complete %s\n", output_filename);
    
    free(image);
    free(gray_image);
    free(filtered_image);
    free(output_image);
    
    return 0;
}
