// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring> // for std::memcpy
#include <numeric>
#include "video_core/command_classes/codecs/vp9.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"

namespace Tegra::Decoder {

// Default compressed header probabilities once frame context resets
constexpr Vp9EntropyProbs default_probs{
    .y_mode_prob{
        65,  32, 18, 144, 162, 194, 41, 51, 98, 132, 68,  18, 165, 217, 196, 45, 40, 78,
        173, 80, 19, 176, 240, 193, 64, 35, 46, 221, 135, 38, 194, 248, 121, 96, 85, 29,
    },
    .partition_prob{
        199, 122, 141, 0, 147, 63, 159, 0, 148, 133, 118, 0, 121, 104, 114, 0,
        174, 73,  87,  0, 92,  41, 83,  0, 82,  99,  50,  0, 53,  39,  39,  0,
        177, 58,  59,  0, 68,  26, 63,  0, 52,  79,  25,  0, 17,  14,  12,  0,
        222, 34,  30,  0, 72,  16, 44,  0, 58,  32,  12,  0, 10,  7,   6,   0,
    },
    .coef_probs{
        195, 29,  183, 0, 84,  49,  136, 0, 8,   42,  71,  0, 0,   0,   0,   0, 0,   0,   0,   0,
        0,   0,   0,   0, 31,  107, 169, 0, 35,  99,  159, 0, 17,  82,  140, 0, 8,   66,  114, 0,
        2,   44,  76,  0, 1,   19,  32,  0, 40,  132, 201, 0, 29,  114, 187, 0, 13,  91,  157, 0,
        7,   75,  127, 0, 3,   58,  95,  0, 1,   28,  47,  0, 69,  142, 221, 0, 42,  122, 201, 0,
        15,  91,  159, 0, 6,   67,  121, 0, 1,   42,  77,  0, 1,   17,  31,  0, 102, 148, 228, 0,
        67,  117, 204, 0, 17,  82,  154, 0, 6,   59,  114, 0, 2,   39,  75,  0, 1,   15,  29,  0,
        156, 57,  233, 0, 119, 57,  212, 0, 58,  48,  163, 0, 29,  40,  124, 0, 12,  30,  81,  0,
        3,   12,  31,  0, 191, 107, 226, 0, 124, 117, 204, 0, 25,  99,  155, 0, 0,   0,   0,   0,
        0,   0,   0,   0, 0,   0,   0,   0, 29,  148, 210, 0, 37,  126, 194, 0, 8,   93,  157, 0,
        2,   68,  118, 0, 1,   39,  69,  0, 1,   17,  33,  0, 41,  151, 213, 0, 27,  123, 193, 0,
        3,   82,  144, 0, 1,   58,  105, 0, 1,   32,  60,  0, 1,   13,  26,  0, 59,  159, 220, 0,
        23,  126, 198, 0, 4,   88,  151, 0, 1,   66,  114, 0, 1,   38,  71,  0, 1,   18,  34,  0,
        114, 136, 232, 0, 51,  114, 207, 0, 11,  83,  155, 0, 3,   56,  105, 0, 1,   33,  65,  0,
        1,   17,  34,  0, 149, 65,  234, 0, 121, 57,  215, 0, 61,  49,  166, 0, 28,  36,  114, 0,
        12,  25,  76,  0, 3,   16,  42,  0, 214, 49,  220, 0, 132, 63,  188, 0, 42,  65,  137, 0,
        0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0, 85,  137, 221, 0, 104, 131, 216, 0,
        49,  111, 192, 0, 21,  87,  155, 0, 2,   49,  87,  0, 1,   16,  28,  0, 89,  163, 230, 0,
        90,  137, 220, 0, 29,  100, 183, 0, 10,  70,  135, 0, 2,   42,  81,  0, 1,   17,  33,  0,
        108, 167, 237, 0, 55,  133, 222, 0, 15,  97,  179, 0, 4,   72,  135, 0, 1,   45,  85,  0,
        1,   19,  38,  0, 124, 146, 240, 0, 66,  124, 224, 0, 17,  88,  175, 0, 4,   58,  122, 0,
        1,   36,  75,  0, 1,   18,  37,  0, 141, 79,  241, 0, 126, 70,  227, 0, 66,  58,  182, 0,
        30,  44,  136, 0, 12,  34,  96,  0, 2,   20,  47,  0, 229, 99,  249, 0, 143, 111, 235, 0,
        46,  109, 192, 0, 0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0, 82,  158, 236, 0,
        94,  146, 224, 0, 25,  117, 191, 0, 9,   87,  149, 0, 3,   56,  99,  0, 1,   33,  57,  0,
        83,  167, 237, 0, 68,  145, 222, 0, 10,  103, 177, 0, 2,   72,  131, 0, 1,   41,  79,  0,
        1,   20,  39,  0, 99,  167, 239, 0, 47,  141, 224, 0, 10,  104, 178, 0, 2,   73,  133, 0,
        1,   44,  85,  0, 1,   22,  47,  0, 127, 145, 243, 0, 71,  129, 228, 0, 17,  93,  177, 0,
        3,   61,  124, 0, 1,   41,  84,  0, 1,   21,  52,  0, 157, 78,  244, 0, 140, 72,  231, 0,
        69,  58,  184, 0, 31,  44,  137, 0, 14,  38,  105, 0, 8,   23,  61,  0, 125, 34,  187, 0,
        52,  41,  133, 0, 6,   31,  56,  0, 0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0,
        37,  109, 153, 0, 51,  102, 147, 0, 23,  87,  128, 0, 8,   67,  101, 0, 1,   41,  63,  0,
        1,   19,  29,  0, 31,  154, 185, 0, 17,  127, 175, 0, 6,   96,  145, 0, 2,   73,  114, 0,
        1,   51,  82,  0, 1,   28,  45,  0, 23,  163, 200, 0, 10,  131, 185, 0, 2,   93,  148, 0,
        1,   67,  111, 0, 1,   41,  69,  0, 1,   14,  24,  0, 29,  176, 217, 0, 12,  145, 201, 0,
        3,   101, 156, 0, 1,   69,  111, 0, 1,   39,  63,  0, 1,   14,  23,  0, 57,  192, 233, 0,
        25,  154, 215, 0, 6,   109, 167, 0, 3,   78,  118, 0, 1,   48,  69,  0, 1,   21,  29,  0,
        202, 105, 245, 0, 108, 106, 216, 0, 18,  90,  144, 0, 0,   0,   0,   0, 0,   0,   0,   0,
        0,   0,   0,   0, 33,  172, 219, 0, 64,  149, 206, 0, 14,  117, 177, 0, 5,   90,  141, 0,
        2,   61,  95,  0, 1,   37,  57,  0, 33,  179, 220, 0, 11,  140, 198, 0, 1,   89,  148, 0,
        1,   60,  104, 0, 1,   33,  57,  0, 1,   12,  21,  0, 30,  181, 221, 0, 8,   141, 198, 0,
        1,   87,  145, 0, 1,   58,  100, 0, 1,   31,  55,  0, 1,   12,  20,  0, 32,  186, 224, 0,
        7,   142, 198, 0, 1,   86,  143, 0, 1,   58,  100, 0, 1,   31,  55,  0, 1,   12,  22,  0,
        57,  192, 227, 0, 20,  143, 204, 0, 3,   96,  154, 0, 1,   68,  112, 0, 1,   42,  69,  0,
        1,   19,  32,  0, 212, 35,  215, 0, 113, 47,  169, 0, 29,  48,  105, 0, 0,   0,   0,   0,
        0,   0,   0,   0, 0,   0,   0,   0, 74,  129, 203, 0, 106, 120, 203, 0, 49,  107, 178, 0,
        19,  84,  144, 0, 4,   50,  84,  0, 1,   15,  25,  0, 71,  172, 217, 0, 44,  141, 209, 0,
        15,  102, 173, 0, 6,   76,  133, 0, 2,   51,  89,  0, 1,   24,  42,  0, 64,  185, 231, 0,
        31,  148, 216, 0, 8,   103, 175, 0, 3,   74,  131, 0, 1,   46,  81,  0, 1,   18,  30,  0,
        65,  196, 235, 0, 25,  157, 221, 0, 5,   105, 174, 0, 1,   67,  120, 0, 1,   38,  69,  0,
        1,   15,  30,  0, 65,  204, 238, 0, 30,  156, 224, 0, 7,   107, 177, 0, 2,   70,  124, 0,
        1,   42,  73,  0, 1,   18,  34,  0, 225, 86,  251, 0, 144, 104, 235, 0, 42,  99,  181, 0,
        0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0, 85,  175, 239, 0, 112, 165, 229, 0,
        29,  136, 200, 0, 12,  103, 162, 0, 6,   77,  123, 0, 2,   53,  84,  0, 75,  183, 239, 0,
        30,  155, 221, 0, 3,   106, 171, 0, 1,   74,  128, 0, 1,   44,  76,  0, 1,   17,  28,  0,
        73,  185, 240, 0, 27,  159, 222, 0, 2,   107, 172, 0, 1,   75,  127, 0, 1,   42,  73,  0,
        1,   17,  29,  0, 62,  190, 238, 0, 21,  159, 222, 0, 2,   107, 172, 0, 1,   72,  122, 0,
        1,   40,  71,  0, 1,   18,  32,  0, 61,  199, 240, 0, 27,  161, 226, 0, 4,   113, 180, 0,
        1,   76,  129, 0, 1,   46,  80,  0, 1,   23,  41,  0, 7,   27,  153, 0, 5,   30,  95,  0,
        1,   16,  30,  0, 0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0, 50,  75,  127, 0,
        57,  75,  124, 0, 27,  67,  108, 0, 10,  54,  86,  0, 1,   33,  52,  0, 1,   12,  18,  0,
        43,  125, 151, 0, 26,  108, 148, 0, 7,   83,  122, 0, 2,   59,  89,  0, 1,   38,  60,  0,
        1,   17,  27,  0, 23,  144, 163, 0, 13,  112, 154, 0, 2,   75,  117, 0, 1,   50,  81,  0,
        1,   31,  51,  0, 1,   14,  23,  0, 18,  162, 185, 0, 6,   123, 171, 0, 1,   78,  125, 0,
        1,   51,  86,  0, 1,   31,  54,  0, 1,   14,  23,  0, 15,  199, 227, 0, 3,   150, 204, 0,
        1,   91,  146, 0, 1,   55,  95,  0, 1,   30,  53,  0, 1,   11,  20,  0, 19,  55,  240, 0,
        19,  59,  196, 0, 3,   52,  105, 0, 0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0,
        41,  166, 207, 0, 104, 153, 199, 0, 31,  123, 181, 0, 14,  101, 152, 0, 5,   72,  106, 0,
        1,   36,  52,  0, 35,  176, 211, 0, 12,  131, 190, 0, 2,   88,  144, 0, 1,   60,  101, 0,
        1,   36,  60,  0, 1,   16,  28,  0, 28,  183, 213, 0, 8,   134, 191, 0, 1,   86,  142, 0,
        1,   56,  96,  0, 1,   30,  53,  0, 1,   12,  20,  0, 20,  190, 215, 0, 4,   135, 192, 0,
        1,   84,  139, 0, 1,   53,  91,  0, 1,   28,  49,  0, 1,   11,  20,  0, 13,  196, 216, 0,
        2,   137, 192, 0, 1,   86,  143, 0, 1,   57,  99,  0, 1,   32,  56,  0, 1,   13,  24,  0,
        211, 29,  217, 0, 96,  47,  156, 0, 22,  43,  87,  0, 0,   0,   0,   0, 0,   0,   0,   0,
        0,   0,   0,   0, 78,  120, 193, 0, 111, 116, 186, 0, 46,  102, 164, 0, 15,  80,  128, 0,
        2,   49,  76,  0, 1,   18,  28,  0, 71,  161, 203, 0, 42,  132, 192, 0, 10,  98,  150, 0,
        3,   69,  109, 0, 1,   44,  70,  0, 1,   18,  29,  0, 57,  186, 211, 0, 30,  140, 196, 0,
        4,   93,  146, 0, 1,   62,  102, 0, 1,   38,  65,  0, 1,   16,  27,  0, 47,  199, 217, 0,
        14,  145, 196, 0, 1,   88,  142, 0, 1,   57,  98,  0, 1,   36,  62,  0, 1,   15,  26,  0,
        26,  219, 229, 0, 5,   155, 207, 0, 1,   94,  151, 0, 1,   60,  104, 0, 1,   36,  62,  0,
        1,   16,  28,  0, 233, 29,  248, 0, 146, 47,  220, 0, 43,  52,  140, 0, 0,   0,   0,   0,
        0,   0,   0,   0, 0,   0,   0,   0, 100, 163, 232, 0, 179, 161, 222, 0, 63,  142, 204, 0,
        37,  113, 174, 0, 26,  89,  137, 0, 18,  68,  97,  0, 85,  181, 230, 0, 32,  146, 209, 0,
        7,   100, 164, 0, 3,   71,  121, 0, 1,   45,  77,  0, 1,   18,  30,  0, 65,  187, 230, 0,
        20,  148, 207, 0, 2,   97,  159, 0, 1,   68,  116, 0, 1,   40,  70,  0, 1,   14,  29,  0,
        40,  194, 227, 0, 8,   147, 204, 0, 1,   94,  155, 0, 1,   65,  112, 0, 1,   39,  66,  0,
        1,   14,  26,  0, 16,  208, 228, 0, 3,   151, 207, 0, 1,   98,  160, 0, 1,   67,  117, 0,
        1,   41,  74,  0, 1,   17,  31,  0, 17,  38,  140, 0, 7,   34,  80,  0, 1,   17,  29,  0,
        0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0, 37,  75,  128, 0, 41,  76,  128, 0,
        26,  66,  116, 0, 12,  52,  94,  0, 2,   32,  55,  0, 1,   10,  16,  0, 50,  127, 154, 0,
        37,  109, 152, 0, 16,  82,  121, 0, 5,   59,  85,  0, 1,   35,  54,  0, 1,   13,  20,  0,
        40,  142, 167, 0, 17,  110, 157, 0, 2,   71,  112, 0, 1,   44,  72,  0, 1,   27,  45,  0,
        1,   11,  17,  0, 30,  175, 188, 0, 9,   124, 169, 0, 1,   74,  116, 0, 1,   48,  78,  0,
        1,   30,  49,  0, 1,   11,  18,  0, 10,  222, 223, 0, 2,   150, 194, 0, 1,   83,  128, 0,
        1,   48,  79,  0, 1,   27,  45,  0, 1,   11,  17,  0, 36,  41,  235, 0, 29,  36,  193, 0,
        10,  27,  111, 0, 0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0, 85,  165, 222, 0,
        177, 162, 215, 0, 110, 135, 195, 0, 57,  113, 168, 0, 23,  83,  120, 0, 10,  49,  61,  0,
        85,  190, 223, 0, 36,  139, 200, 0, 5,   90,  146, 0, 1,   60,  103, 0, 1,   38,  65,  0,
        1,   18,  30,  0, 72,  202, 223, 0, 23,  141, 199, 0, 2,   86,  140, 0, 1,   56,  97,  0,
        1,   36,  61,  0, 1,   16,  27,  0, 55,  218, 225, 0, 13,  145, 200, 0, 1,   86,  141, 0,
        1,   57,  99,  0, 1,   35,  61,  0, 1,   13,  22,  0, 15,  235, 212, 0, 1,   132, 184, 0,
        1,   84,  139, 0, 1,   57,  97,  0, 1,   34,  56,  0, 1,   14,  23,  0, 181, 21,  201, 0,
        61,  37,  123, 0, 10,  38,  71,  0, 0,   0,   0,   0, 0,   0,   0,   0, 0,   0,   0,   0,
        47,  106, 172, 0, 95,  104, 173, 0, 42,  93,  159, 0, 18,  77,  131, 0, 4,   50,  81,  0,
        1,   17,  23,  0, 62,  147, 199, 0, 44,  130, 189, 0, 28,  102, 154, 0, 18,  75,  115, 0,
        2,   44,  65,  0, 1,   12,  19,  0, 55,  153, 210, 0, 24,  130, 194, 0, 3,   93,  146, 0,
        1,   61,  97,  0, 1,   31,  50,  0, 1,   10,  16,  0, 49,  186, 223, 0, 17,  148, 204, 0,
        1,   96,  142, 0, 1,   53,  83,  0, 1,   26,  44,  0, 1,   11,  17,  0, 13,  217, 212, 0,
        2,   136, 180, 0, 1,   78,  124, 0, 1,   50,  83,  0, 1,   29,  49,  0, 1,   14,  23,  0,
        197, 13,  247, 0, 82,  17,  222, 0, 25,  17,  162, 0, 0,   0,   0,   0, 0,   0,   0,   0,
        0,   0,   0,   0, 126, 186, 247, 0, 234, 191, 243, 0, 176, 177, 234, 0, 104, 158, 220, 0,
        66,  128, 186, 0, 55,  90,  137, 0, 111, 197, 242, 0, 46,  158, 219, 0, 9,   104, 171, 0,
        2,   65,  125, 0, 1,   44,  80,  0, 1,   17,  91,  0, 104, 208, 245, 0, 39,  168, 224, 0,
        3,   109, 162, 0, 1,   79,  124, 0, 1,   50,  102, 0, 1,   43,  102, 0, 84,  220, 246, 0,
        31,  177, 231, 0, 2,   115, 180, 0, 1,   79,  134, 0, 1,   55,  77,  0, 1,   60,  79,  0,
        43,  243, 240, 0, 8,   180, 217, 0, 1,   115, 166, 0, 1,   84,  121, 0, 1,   51,  67,  0,
        1,   16,  6,   0,
    },
    .switchable_interp_prob{235, 162, 36, 255, 34, 3, 149, 144},
    .inter_mode_prob{
        2,  173, 34, 0,  7,  145, 85, 0,  7,  166, 63, 0,  7,  94,
        66, 0,   8,  64, 46, 0,   17, 81, 31, 0,   25, 29, 30, 0,
    },
    .intra_inter_prob{9, 102, 187, 225},
    .comp_inter_prob{9, 102, 187, 225, 0},
    .single_ref_prob{33, 16, 77, 74, 142, 142, 172, 170, 238, 247},
    .comp_ref_prob{50, 126, 123, 221, 226},
    .tx_32x32_prob{3, 136, 37, 5, 52, 13},
    .tx_16x16_prob{20, 152, 15, 101},
    .tx_8x8_prob{100, 66},
    .skip_probs{192, 128, 64},
    .joints{32, 64, 96},
    .sign{128, 128},
    .classes{
        224, 144, 192, 168, 192, 176, 192, 198, 198, 245,
        216, 128, 176, 160, 176, 176, 192, 198, 198, 208,
    },
    .class_0{216, 208},
    .prob_bits{
        136, 140, 148, 160, 176, 192, 224, 234, 234, 240,
        136, 140, 148, 160, 176, 192, 224, 234, 234, 240,
    },
    .class_0_fr{128, 128, 64, 96, 112, 64, 128, 128, 64, 96, 112, 64},
    .fr{64, 96, 64, 64, 96, 64},
    .class_0_hp{160, 160},
    .high_precision{128, 128},
};

VP9::VP9(GPU& gpu) : gpu(gpu) {}

VP9::~VP9() = default;

void VP9::WriteProbabilityUpdate(VpxRangeEncoder& writer, u8 new_prob, u8 old_prob) {
    const bool update = new_prob != old_prob;

    writer.Write(update, diff_update_probability);

    if (update) {
        WriteProbabilityDelta(writer, new_prob, old_prob);
    }
}
template <typename T, std::size_t N>
void VP9::WriteProbabilityUpdate(VpxRangeEncoder& writer, const std::array<T, N>& new_prob,
                                 const std::array<T, N>& old_prob) {
    for (std::size_t offset = 0; offset < new_prob.size(); ++offset) {
        WriteProbabilityUpdate(writer, new_prob[offset], old_prob[offset]);
    }
}

template <typename T, std::size_t N>
void VP9::WriteProbabilityUpdateAligned4(VpxRangeEncoder& writer, const std::array<T, N>& new_prob,
                                         const std::array<T, N>& old_prob) {
    for (std::size_t offset = 0; offset < new_prob.size(); offset += 4) {
        WriteProbabilityUpdate(writer, new_prob[offset + 0], old_prob[offset + 0]);
        WriteProbabilityUpdate(writer, new_prob[offset + 1], old_prob[offset + 1]);
        WriteProbabilityUpdate(writer, new_prob[offset + 2], old_prob[offset + 2]);
    }
}

void VP9::WriteProbabilityDelta(VpxRangeEncoder& writer, u8 new_prob, u8 old_prob) {
    const int delta = RemapProbability(new_prob, old_prob);

    EncodeTermSubExp(writer, delta);
}

s32 VP9::RemapProbability(s32 new_prob, s32 old_prob) {
    new_prob--;
    old_prob--;

    std::size_t index{};

    if (old_prob * 2 <= 0xff) {
        index = static_cast<std::size_t>(std::max(0, RecenterNonNeg(new_prob, old_prob) - 1));
    } else {
        index = static_cast<std::size_t>(
            std::max(0, RecenterNonNeg(0xff - 1 - new_prob, 0xff - 1 - old_prob) - 1));
    }

    return map_lut[index];
}

s32 VP9::RecenterNonNeg(s32 new_prob, s32 old_prob) {
    if (new_prob > old_prob * 2) {
        return new_prob;
    } else if (new_prob >= old_prob) {
        return (new_prob - old_prob) * 2;
    } else {
        return (old_prob - new_prob) * 2 - 1;
    }
}

void VP9::EncodeTermSubExp(VpxRangeEncoder& writer, s32 value) {
    if (WriteLessThan(writer, value, 16)) {
        writer.Write(value, 4);
    } else if (WriteLessThan(writer, value, 32)) {
        writer.Write(value - 16, 4);
    } else if (WriteLessThan(writer, value, 64)) {
        writer.Write(value - 32, 5);
    } else {
        value -= 64;

        constexpr s32 size = 8;

        const s32 mask = (1 << size) - 191;

        const s32 delta = value - mask;

        if (delta < 0) {
            writer.Write(value, size - 1);
        } else {
            writer.Write(delta / 2 + mask, size - 1);
            writer.Write(delta & 1, 1);
        }
    }
}

bool VP9::WriteLessThan(VpxRangeEncoder& writer, s32 value, s32 test) {
    const bool is_lt = value < test;
    writer.Write(!is_lt);
    return is_lt;
}

void VP9::WriteCoefProbabilityUpdate(VpxRangeEncoder& writer, s32 tx_mode,
                                     const std::array<u8, 2304>& new_prob,
                                     const std::array<u8, 2304>& old_prob) {
    // Note: There's 1 byte added on each packet for alignment,
    // this byte is ignored when doing updates.
    constexpr s32 block_bytes = 2 * 2 * 6 * 6 * 4;

    const auto needs_update = [&](s32 base_index) -> bool {
        s32 index = base_index;
        for (s32 i = 0; i < 2; i++) {
            for (s32 j = 0; j < 2; j++) {
                for (s32 k = 0; k < 6; k++) {
                    for (s32 l = 0; l < 6; l++) {
                        if (new_prob[index + 0] != old_prob[index + 0] ||
                            new_prob[index + 1] != old_prob[index + 1] ||
                            new_prob[index + 2] != old_prob[index + 2]) {
                            return true;
                        }

                        index += 4;
                    }
                }
            }
        }
        return false;
    };

    for (s32 block_index = 0; block_index < 4; block_index++) {
        const s32 base_index = block_index * block_bytes;
        const bool update = needs_update(base_index);
        writer.Write(update);

        if (update) {
            s32 index = base_index;
            for (s32 i = 0; i < 2; i++) {
                for (s32 j = 0; j < 2; j++) {
                    for (s32 k = 0; k < 6; k++) {
                        for (s32 l = 0; l < 6; l++) {
                            if (k != 0 || l < 3) {
                                WriteProbabilityUpdate(writer, new_prob[index + 0],
                                                       old_prob[index + 0]);
                                WriteProbabilityUpdate(writer, new_prob[index + 1],
                                                       old_prob[index + 1]);
                                WriteProbabilityUpdate(writer, new_prob[index + 2],
                                                       old_prob[index + 2]);
                            }
                            index += 4;
                        }
                    }
                }
            }
        }

        if (block_index == tx_mode) {
            break;
        }
    }
}

void VP9::WriteMvProbabilityUpdate(VpxRangeEncoder& writer, u8 new_prob, u8 old_prob) {
    const bool update = new_prob != old_prob;
    writer.Write(update, diff_update_probability);

    if (update) {
        writer.Write(new_prob >> 1, 7);
    }
}

s32 VP9::CalcMinLog2TileCols(s32 frame_width) {
    const s32 sb64_cols = (frame_width + 63) / 64;
    s32 min_log2 = 0;

    while ((64 << min_log2) < sb64_cols) {
        min_log2++;
    }

    return min_log2;
}

s32 VP9::CalcMaxLog2TileCols(s32 frameWidth) {
    const s32 sb64_cols = (frameWidth + 63) / 64;
    s32 max_log2 = 1;

    while ((sb64_cols >> max_log2) >= 4) {
        max_log2++;
    }

    return max_log2 - 1;
}

Vp9PictureInfo VP9::GetVp9PictureInfo(const NvdecCommon::NvdecRegisters& state) {
    PictureInfo picture_info{};
    gpu.MemoryManager().ReadBlock(state.picture_info_offset, &picture_info, sizeof(PictureInfo));
    Vp9PictureInfo vp9_info = picture_info.Convert();

    InsertEntropy(state.vp9_entropy_probs_offset, vp9_info.entropy);

    // surface_luma_offset[0:3] contains the address of the reference frame offsets in the following
    // order: last, golden, altref, current. It may be worthwhile to track the updates done here
    // to avoid buffering frame data needed for reference frame updating in the header composition.
    std::memcpy(vp9_info.frame_offsets.data(), state.surface_luma_offset.data(), 4 * sizeof(u64));

    return vp9_info;
}

void VP9::InsertEntropy(u64 offset, Vp9EntropyProbs& dst) {
    EntropyProbs entropy{};
    gpu.MemoryManager().ReadBlock(offset, &entropy, sizeof(EntropyProbs));
    entropy.Convert(dst);
}

Vp9FrameContainer VP9::GetCurrentFrame(const NvdecCommon::NvdecRegisters& state) {
    Vp9FrameContainer frame{};
    {
        gpu.SyncGuestHost();
        frame.info = std::move(GetVp9PictureInfo(state));

        frame.bit_stream.resize(frame.info.bitstream_size);
        gpu.MemoryManager().ReadBlock(state.frame_bitstream_offset, frame.bit_stream.data(),
                                      frame.info.bitstream_size);
    }
    // Buffer two frames, saving the last show frame info
    if (next_next_frame.bit_stream.size() != 0) {
        Vp9FrameContainer temp{
            .info = frame.info,
            .bit_stream = frame.bit_stream,
        };
        next_next_frame.info.show_frame = frame.info.last_frame_shown;
        frame.info = next_next_frame.info;
        frame.bit_stream = next_next_frame.bit_stream;
        next_next_frame = std::move(temp);

        if (next_frame.bit_stream.size() != 0) {
            Vp9FrameContainer temp{
                .info = frame.info,
                .bit_stream = frame.bit_stream,
            };
            next_frame.info.show_frame = frame.info.last_frame_shown;
            frame.info = next_frame.info;
            frame.bit_stream = next_frame.bit_stream;
            next_frame = std::move(temp);
        } else {
            next_frame.info = frame.info;
            next_frame.bit_stream = frame.bit_stream;
        }
    } else {
        next_next_frame.info = frame.info;
        next_next_frame.bit_stream = frame.bit_stream;
    }
    return frame;
}

std::vector<u8> VP9::ComposeCompressedHeader() {
    VpxRangeEncoder writer{};

    if (!current_frame_info.lossless) {
        if (static_cast<u32>(current_frame_info.transform_mode) >= 3) {
            writer.Write(3, 2);
            writer.Write(current_frame_info.transform_mode == 4);
        } else {
            writer.Write(current_frame_info.transform_mode, 2);
        }
    }

    if (current_frame_info.transform_mode == 4) {
        // tx_mode_probs() in the spec
        WriteProbabilityUpdate(writer, current_frame_info.entropy.tx_8x8_prob,
                               prev_frame_probs.tx_8x8_prob);
        WriteProbabilityUpdate(writer, current_frame_info.entropy.tx_16x16_prob,
                               prev_frame_probs.tx_16x16_prob);
        WriteProbabilityUpdate(writer, current_frame_info.entropy.tx_32x32_prob,
                               prev_frame_probs.tx_32x32_prob);
        if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
            prev_frame_probs.tx_8x8_prob = current_frame_info.entropy.tx_8x8_prob;
            prev_frame_probs.tx_16x16_prob = current_frame_info.entropy.tx_16x16_prob;
            prev_frame_probs.tx_32x32_prob = current_frame_info.entropy.tx_32x32_prob;
        }
    }
    // read_coef_probs()  in the spec
    WriteCoefProbabilityUpdate(writer, current_frame_info.transform_mode,
                               current_frame_info.entropy.coef_probs, prev_frame_probs.coef_probs);
    // read_skip_probs()  in the spec
    WriteProbabilityUpdate(writer, current_frame_info.entropy.skip_probs,
                           prev_frame_probs.skip_probs);

    if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
        prev_frame_probs.coef_probs = current_frame_info.entropy.coef_probs;
        prev_frame_probs.skip_probs = current_frame_info.entropy.skip_probs;
    }

    if (!current_frame_info.intra_only) {
        // read_inter_probs() in the spec
        WriteProbabilityUpdateAligned4(writer, current_frame_info.entropy.inter_mode_prob,
                                       prev_frame_probs.inter_mode_prob);
        if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
            prev_frame_probs.inter_mode_prob = current_frame_info.entropy.inter_mode_prob;
        }

        if (current_frame_info.interp_filter == 4) {
            // read_interp_filter_probs() in the spec
            WriteProbabilityUpdate(writer, current_frame_info.entropy.switchable_interp_prob,
                                   prev_frame_probs.switchable_interp_prob);
            if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
                prev_frame_probs.switchable_interp_prob =
                    current_frame_info.entropy.switchable_interp_prob;
            }
        }

        // read_is_inter_probs() in the spec
        WriteProbabilityUpdate(writer, current_frame_info.entropy.intra_inter_prob,
                               prev_frame_probs.intra_inter_prob);
        if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
            prev_frame_probs.intra_inter_prob = current_frame_info.entropy.intra_inter_prob;
        }
        // frame_reference_mode() in the spec
        if ((current_frame_info.ref_frame_sign_bias[1] & 1) !=
                (current_frame_info.ref_frame_sign_bias[2] & 1) ||
            (current_frame_info.ref_frame_sign_bias[1] & 1) !=
                (current_frame_info.ref_frame_sign_bias[3] & 1)) {
            if (current_frame_info.reference_mode >= 1) {
                writer.Write(1, 1);
                writer.Write(current_frame_info.reference_mode == 2);
            } else {
                writer.Write(0, 1);
            }
        }

        // frame_reference_mode_probs() in the spec
        if (current_frame_info.reference_mode == 2) {
            WriteProbabilityUpdate(writer, current_frame_info.entropy.comp_inter_prob,
                                   prev_frame_probs.comp_inter_prob);
            if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
                prev_frame_probs.comp_inter_prob = current_frame_info.entropy.comp_inter_prob;
            }
        }

        if (current_frame_info.reference_mode != 1) {
            WriteProbabilityUpdate(writer, current_frame_info.entropy.single_ref_prob,
                                   prev_frame_probs.single_ref_prob);
            if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
                prev_frame_probs.single_ref_prob = current_frame_info.entropy.single_ref_prob;
            }
        }

        if (current_frame_info.reference_mode != 0) {
            WriteProbabilityUpdate(writer, current_frame_info.entropy.comp_ref_prob,
                                   prev_frame_probs.comp_ref_prob);
            if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
                prev_frame_probs.comp_ref_prob = current_frame_info.entropy.comp_ref_prob;
            }
        }

        // read_y_mode_probs
        for (std::size_t index = 0; index < current_frame_info.entropy.y_mode_prob.size();
             ++index) {
            WriteProbabilityUpdate(writer, current_frame_info.entropy.y_mode_prob[index],
                                   prev_frame_probs.y_mode_prob[index]);
        }
        if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
            prev_frame_probs.y_mode_prob = current_frame_info.entropy.y_mode_prob;
        }
        // read_partition_probs
        WriteProbabilityUpdateAligned4(writer, current_frame_info.entropy.partition_prob,
                                       prev_frame_probs.partition_prob);
        if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
            prev_frame_probs.partition_prob = current_frame_info.entropy.partition_prob;
        }

        // mv_probs
        for (s32 i = 0; i < 3; i++) {
            WriteMvProbabilityUpdate(writer, current_frame_info.entropy.joints[i],
                                     prev_frame_probs.joints[i]);
        }
        if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
            prev_frame_probs.joints = current_frame_info.entropy.joints;
        }

        for (s32 i = 0; i < 2; i++) {
            WriteMvProbabilityUpdate(writer, current_frame_info.entropy.sign[i],
                                     prev_frame_probs.sign[i]);

            for (s32 j = 0; j < 10; j++) {
                const int index = i * 10 + j;

                WriteMvProbabilityUpdate(writer, current_frame_info.entropy.classes[index],
                                         prev_frame_probs.classes[index]);
            }

            WriteMvProbabilityUpdate(writer, current_frame_info.entropy.class_0[i],
                                     prev_frame_probs.class_0[i]);

            for (s32 j = 0; j < 10; j++) {
                const int index = i * 10 + j;

                WriteMvProbabilityUpdate(writer, current_frame_info.entropy.prob_bits[index],
                                         prev_frame_probs.prob_bits[index]);
            }
        }

        for (s32 i = 0; i < 2; i++) {
            for (s32 j = 0; j < 2; j++) {
                for (s32 k = 0; k < 3; k++) {
                    const int index = i * 2 * 3 + j * 3 + k;

                    WriteMvProbabilityUpdate(writer, current_frame_info.entropy.class_0_fr[index],
                                             prev_frame_probs.class_0_fr[index]);
                }
            }

            for (s32 j = 0; j < 3; j++) {
                const int index = i * 3 + j;

                WriteMvProbabilityUpdate(writer, current_frame_info.entropy.fr[index],
                                         prev_frame_probs.fr[index]);
            }
        }

        if (current_frame_info.allow_high_precision_mv) {
            for (s32 index = 0; index < 2; index++) {
                WriteMvProbabilityUpdate(writer, current_frame_info.entropy.class_0_hp[index],
                                         prev_frame_probs.class_0_hp[index]);
                WriteMvProbabilityUpdate(writer, current_frame_info.entropy.high_precision[index],
                                         prev_frame_probs.high_precision[index]);
            }
        }

        // save previous probs
        if (current_frame_info.show_frame && !current_frame_info.is_key_frame) {
            prev_frame_probs.sign = current_frame_info.entropy.sign;
            prev_frame_probs.classes = current_frame_info.entropy.classes;
            prev_frame_probs.class_0 = current_frame_info.entropy.class_0;
            prev_frame_probs.prob_bits = current_frame_info.entropy.prob_bits;
            prev_frame_probs.class_0_fr = current_frame_info.entropy.class_0_fr;
            prev_frame_probs.fr = current_frame_info.entropy.fr;
            prev_frame_probs.class_0_hp = current_frame_info.entropy.class_0_hp;
            prev_frame_probs.high_precision = current_frame_info.entropy.high_precision;
        }
    }

    writer.End();
    return writer.GetBuffer();

    const auto writer_bytearray = writer.GetBuffer();

    std::vector<u8> compressed_header(writer_bytearray.size());
    std::memcpy(compressed_header.data(), writer_bytearray.data(), writer_bytearray.size());
    return compressed_header;
}

VpxBitStreamWriter VP9::ComposeUncompressedHeader() {
    VpxBitStreamWriter uncomp_writer{};

    uncomp_writer.WriteU(2, 2);                                      // Frame marker.
    uncomp_writer.WriteU(0, 2);                                      // Profile.
    uncomp_writer.WriteBit(false);                                   // Show existing frame.
    uncomp_writer.WriteBit(!current_frame_info.is_key_frame);        // is key frame?
    uncomp_writer.WriteBit(current_frame_info.show_frame);           // show frame?
    uncomp_writer.WriteBit(current_frame_info.error_resilient_mode); // error reslience

    if (current_frame_info.is_key_frame) {
        uncomp_writer.WriteU(frame_sync_code, 24);
        uncomp_writer.WriteU(0, 3); // Color space.
        uncomp_writer.WriteU(0, 1); // Color range.
        uncomp_writer.WriteU(current_frame_info.frame_size.width - 1, 16);
        uncomp_writer.WriteU(current_frame_info.frame_size.height - 1, 16);
        uncomp_writer.WriteBit(false); // Render and frame size different.

        // Reset context
        prev_frame_probs = default_probs;
        swap_next_golden = false;
        loop_filter_ref_deltas.fill(0);
        loop_filter_mode_deltas.fill(0);

        // allow frames offsets to stabilize before checking for golden frames
        grace_period = 4;

        // On key frames, all frame slots are set to the current frame,
        // so the value of the selected slot doesn't really matter.
        frame_ctxs.fill({current_frame_number, false, default_probs});

        // intra only, meaning the frame can be recreated with no other references
        current_frame_info.intra_only = true;

    } else {
        std::array<s32, 3> ref_frame_index;

        if (!current_frame_info.show_frame) {
            uncomp_writer.WriteBit(current_frame_info.intra_only);
            if (!current_frame_info.last_frame_was_key) {
                swap_next_golden = !swap_next_golden;
            }
        } else {
            current_frame_info.intra_only = false;
        }
        if (!current_frame_info.error_resilient_mode) {
            uncomp_writer.WriteU(0, 2); // Reset frame context.
        }

        // Last, Golden, Altref frames
        ref_frame_index = std::array<s32, 3>{0, 1, 2};

        // set when next frame is hidden
        // altref and golden references are swapped
        if (swap_next_golden) {
            ref_frame_index = std::array<s32, 3>{0, 2, 1};
        }

        // update Last Frame
        u64 refresh_frame_flags = 1;

        // golden frame may refresh, determined if the next golden frame offset is changed
        bool golden_refresh = false;
        if (grace_period <= 0) {
            for (s32 index = 1; index < 3; ++index) {
                if (current_frame_info.frame_offsets[index] !=
                    next_frame.info.frame_offsets[index]) {
                    current_frame_info.refresh_frame[index] = true;
                    golden_refresh = true;
                    grace_period = 3;
                }
            }
        }

        if (current_frame_info.show_frame &&
            (!next_frame.info.show_frame || next_frame.info.is_key_frame)) {
            // Update golden frame
            refresh_frame_flags = swap_next_golden ? 2 : 4;
        }

        if (!current_frame_info.show_frame) {
            // Update altref
            refresh_frame_flags = swap_next_golden ? 2 : 4;
        } else if (golden_refresh) {
            refresh_frame_flags = 3;
        }

        if (current_frame_info.intra_only) {
            uncomp_writer.WriteU(frame_sync_code, 24);
            uncomp_writer.WriteU(static_cast<s32>(refresh_frame_flags), 8);
            uncomp_writer.WriteU(current_frame_info.frame_size.width - 1, 16);
            uncomp_writer.WriteU(current_frame_info.frame_size.height - 1, 16);
            uncomp_writer.WriteBit(false); // Render and frame size different.
        } else {
            uncomp_writer.WriteU(static_cast<s32>(refresh_frame_flags), 8);

            for (s32 index = 1; index < 4; index++) {
                uncomp_writer.WriteU(ref_frame_index[index - 1], 3);
                uncomp_writer.WriteU(current_frame_info.ref_frame_sign_bias[index], 1);
            }

            uncomp_writer.WriteBit(true);  // Frame size with refs.
            uncomp_writer.WriteBit(false); // Render and frame size different.
            uncomp_writer.WriteBit(current_frame_info.allow_high_precision_mv);
            uncomp_writer.WriteBit(current_frame_info.interp_filter == 4);

            if (current_frame_info.interp_filter != 4) {
                uncomp_writer.WriteU(current_frame_info.interp_filter, 2);
            }
        }
    }

    if (!current_frame_info.error_resilient_mode) {
        uncomp_writer.WriteBit(true); // Refresh frame context. where do i get this info from?
        uncomp_writer.WriteBit(true); // Frame parallel decoding mode.
    }

    int frame_ctx_idx = 0;
    if (!current_frame_info.show_frame) {
        frame_ctx_idx = 1;
    }

    uncomp_writer.WriteU(frame_ctx_idx, 2); // Frame context index.
    prev_frame_probs =
        frame_ctxs[frame_ctx_idx].probs; // reference probabilities for compressed header
    frame_ctxs[frame_ctx_idx] = {current_frame_number, false, current_frame_info.entropy};

    uncomp_writer.WriteU(current_frame_info.first_level, 6);
    uncomp_writer.WriteU(current_frame_info.sharpness_level, 3);
    uncomp_writer.WriteBit(current_frame_info.mode_ref_delta_enabled);

    if (current_frame_info.mode_ref_delta_enabled) {
        // check if ref deltas are different, update accordingly
        std::array<bool, 4> update_loop_filter_ref_deltas;
        std::array<bool, 2> update_loop_filter_mode_deltas;

        bool loop_filter_delta_update = false;

        for (std::size_t index = 0; index < current_frame_info.ref_deltas.size(); index++) {
            const s8 old_deltas = loop_filter_ref_deltas[index];
            const s8 new_deltas = current_frame_info.ref_deltas[index];

            loop_filter_delta_update |=
                (update_loop_filter_ref_deltas[index] = old_deltas != new_deltas);
        }

        for (std::size_t index = 0; index < current_frame_info.mode_deltas.size(); index++) {
            const s8 old_deltas = loop_filter_mode_deltas[index];
            const s8 new_deltas = current_frame_info.mode_deltas[index];

            loop_filter_delta_update |=
                (update_loop_filter_mode_deltas[index] = old_deltas != new_deltas);
        }

        uncomp_writer.WriteBit(loop_filter_delta_update);

        if (loop_filter_delta_update) {
            for (std::size_t index = 0; index < current_frame_info.ref_deltas.size(); index++) {
                uncomp_writer.WriteBit(update_loop_filter_ref_deltas[index]);

                if (update_loop_filter_ref_deltas[index]) {
                    uncomp_writer.WriteS(current_frame_info.ref_deltas[index], 6);
                }
            }

            for (std::size_t index = 0; index < current_frame_info.mode_deltas.size(); index++) {
                uncomp_writer.WriteBit(update_loop_filter_mode_deltas[index]);

                if (update_loop_filter_mode_deltas[index]) {
                    uncomp_writer.WriteS(current_frame_info.mode_deltas[index], 6);
                }
            }
            // save new deltas
            loop_filter_ref_deltas = current_frame_info.ref_deltas;
            loop_filter_mode_deltas = current_frame_info.mode_deltas;
        }
    }

    uncomp_writer.WriteU(current_frame_info.base_q_index, 8);

    uncomp_writer.WriteDeltaQ(current_frame_info.y_dc_delta_q);
    uncomp_writer.WriteDeltaQ(current_frame_info.uv_dc_delta_q);
    uncomp_writer.WriteDeltaQ(current_frame_info.uv_ac_delta_q);

    uncomp_writer.WriteBit(false); // Segmentation enabled (TODO).

    const s32 min_tile_cols_log2 = CalcMinLog2TileCols(current_frame_info.frame_size.width);
    const s32 max_tile_cols_log2 = CalcMaxLog2TileCols(current_frame_info.frame_size.width);

    const s32 tile_cols_log2_diff = current_frame_info.log2_tile_cols - min_tile_cols_log2;
    const s32 tile_cols_log2_inc_mask = (1 << tile_cols_log2_diff) - 1;

    // If it's less than the maximum, we need to add an extra 0 on the bitstream
    // to indicate that it should stop reading.
    if (current_frame_info.log2_tile_cols < max_tile_cols_log2) {
        uncomp_writer.WriteU(tile_cols_log2_inc_mask << 1, tile_cols_log2_diff + 1);
    } else {
        uncomp_writer.WriteU(tile_cols_log2_inc_mask, tile_cols_log2_diff);
    }

    const bool tile_rows_log2_is_nonzero = current_frame_info.log2_tile_rows != 0;

    uncomp_writer.WriteBit(tile_rows_log2_is_nonzero);

    if (tile_rows_log2_is_nonzero) {
        uncomp_writer.WriteBit(current_frame_info.log2_tile_rows > 1);
    }

    return uncomp_writer;
}

std::vector<u8>& VP9::ComposeFrameHeader(NvdecCommon::NvdecRegisters& state) {
    std::vector<u8> bitstream;
    {
        Vp9FrameContainer curr_frame = GetCurrentFrame(state);
        current_frame_info = curr_frame.info;
        bitstream = curr_frame.bit_stream;
    }

    // The uncompressed header routine sets PrevProb parameters needed for the compressed header
    auto uncomp_writer = ComposeUncompressedHeader();
    std::vector<u8> compressed_header = ComposeCompressedHeader();

    uncomp_writer.WriteU(static_cast<s32>(compressed_header.size()), 16);
    uncomp_writer.Flush();
    std::vector<u8> uncompressed_header = uncomp_writer.GetByteArray();

    // Write headers and frame to buffer
    frame.resize(uncompressed_header.size() + compressed_header.size() + bitstream.size());
    std::memcpy(frame.data(), uncompressed_header.data(), uncompressed_header.size());
    std::memcpy(frame.data() + uncompressed_header.size(), compressed_header.data(),
                compressed_header.size());
    std::memcpy(frame.data() + uncompressed_header.size() + compressed_header.size(),
                bitstream.data(), bitstream.size());

    // keep track of frame number
    current_frame_number++;
    grace_period--;

    // don't display hidden frames
    hidden = !current_frame_info.show_frame;
    return frame;
}

VpxRangeEncoder::VpxRangeEncoder() {
    Write(false);
}

VpxRangeEncoder::~VpxRangeEncoder() = default;

void VpxRangeEncoder::Write(s32 value, s32 value_size) {
    for (s32 bit = value_size - 1; bit >= 0; bit--) {
        Write(((value >> bit) & 1) != 0);
    }
}

void VpxRangeEncoder::Write(bool bit) {
    Write(bit, half_probability);
}

void VpxRangeEncoder::Write(bool bit, s32 probability) {
    u32 local_range = range;
    const u32 split = 1 + (((local_range - 1) * static_cast<u32>(probability)) >> 8);
    local_range = split;

    if (bit) {
        low_value += split;
        local_range = range - split;
    }

    s32 shift = norm_lut[local_range];
    local_range <<= shift;
    count += shift;

    if (count >= 0) {
        const s32 offset = shift - count;

        if (((low_value << (offset - 1)) >> 31) != 0) {
            const s32 current_pos = static_cast<s32>(base_stream.GetPosition());
            base_stream.Seek(-1, Common::SeekOrigin::FromCurrentPos);
            while (PeekByte() == 0xff) {
                base_stream.WriteByte(0);

                base_stream.Seek(-2, Common::SeekOrigin::FromCurrentPos);
            }
            base_stream.WriteByte(static_cast<u8>((PeekByte() + 1)));
            base_stream.Seek(current_pos, Common::SeekOrigin::SetOrigin);
        }
        base_stream.WriteByte(static_cast<u8>((low_value >> (24 - offset))));

        low_value <<= offset;
        shift = count;
        low_value &= 0xffffff;
        count -= 8;
    }

    low_value <<= shift;
    range = local_range;
}

void VpxRangeEncoder::End() {
    for (std::size_t index = 0; index < 32; ++index) {
        Write(false);
    }
}

u8 VpxRangeEncoder::PeekByte() {
    const u8 value = base_stream.ReadByte();
    base_stream.Seek(-1, Common::SeekOrigin::FromCurrentPos);

    return value;
}

VpxBitStreamWriter::VpxBitStreamWriter() = default;

VpxBitStreamWriter::~VpxBitStreamWriter() = default;

void VpxBitStreamWriter::WriteU(u32 value, u32 value_size) {
    WriteBits(value, value_size);
}

void VpxBitStreamWriter::WriteS(s32 value, u32 value_size) {
    const bool sign = value < 0;
    if (sign) {
        value = -value;
    }

    WriteBits(static_cast<u32>(value << 1) | (sign ? 1 : 0), value_size + 1);
}

void VpxBitStreamWriter::WriteDeltaQ(u32 value) {
    const bool delta_coded = value != 0;
    WriteBit(delta_coded);

    if (delta_coded) {
        WriteBits(value, 4);
    }
}

void VpxBitStreamWriter::WriteBits(u32 value, u32 bit_count) {
    s32 value_pos = 0;
    s32 remaining = bit_count;

    while (remaining > 0) {
        s32 copy_size = remaining;

        const s32 free = GetFreeBufferBits();

        if (copy_size > free) {
            copy_size = free;
        }

        const s32 mask = (1 << copy_size) - 1;

        const s32 src_shift = (bit_count - value_pos) - copy_size;
        const s32 dst_shift = (buffer_size - buffer_pos) - copy_size;

        buffer |= ((value >> src_shift) & mask) << dst_shift;

        value_pos += copy_size;
        buffer_pos += copy_size;
        remaining -= copy_size;
    }
}

void VpxBitStreamWriter::WriteBit(bool state) {
    WriteBits(state ? 1 : 0, 1);
}

s32 VpxBitStreamWriter::GetFreeBufferBits() {
    if (buffer_pos == buffer_size) {
        Flush();
    }

    return buffer_size - buffer_pos;
}

void VpxBitStreamWriter::Flush() {
    if (buffer_pos == 0) {
        return;
    }
    byte_array.push_back(static_cast<u8>(buffer));
    buffer = 0;
    buffer_pos = 0;
}

std::vector<u8>& VpxBitStreamWriter::GetByteArray() {
    return byte_array;
}

const std::vector<u8>& VpxBitStreamWriter::GetByteArray() const {
    return byte_array;
}

} // namespace Tegra::Decoder
