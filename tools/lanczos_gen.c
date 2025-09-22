// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// clang -lm tools/lanczos_gen.c -o tools/lanczos_gen && ./tools/lanczos_gen
#include <stdio.h>
#include <math.h>

double sinc(double x) {
    return x == 0.0f ? 1.0f : sin(M_PI * x) / (M_PI * x);
}

typedef struct vec2 {
    double x;
    double y;
} vec2;

double lanczos(vec2 v, float a) {
    double d = sqrt(v.x * v.x + v.y * v.y);
    return sinc(d) / sinc(d / a);
}

int main(int argc, char* argv[]) {
    const int r = 3; //radius (1 = 3 steps)
    const int k_size = (r * 2 + 1) * (r * 2 + 1);
    double w_sum = 0.0f;
    // kernel size = (r * 2 + 1) ^ 2
    printf("const float w_kernel[%i] = float[] (\n    ", k_size);
    double factor = 1.0f / ((double)r + 1.0f);
    for (int x = -r; x <= r; x++)
        for (int y = -r; y <= r; y++) {
            double w = lanczos((vec2){ .x = x, .y = y }, (double)r);
            printf("%lff, ", w);
            w_sum += w;
        }
    printf("\n);\n");
    printf("const vec2 w_pos[%i] = vec2[] (\n    ", k_size);
    for (int x = -r; x <= r; x++)
        for (int y = -r; y <= r; y++) {
            vec2 kp = (vec2){
                .x = x * factor,
                .y = y * factor
            };
            printf("vec2(%lff, %lff), ", kp.x, kp.y);
        }
    printf("\n);\n");
    printf("const float w_sum = %lff;\n", w_sum);
    return 0;
}
