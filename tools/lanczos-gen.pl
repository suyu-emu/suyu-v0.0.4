#!/usr/bin/perl
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later
use strict;
use warnings;
sub generate_lanczos {
    my $pi = 3.14159265358979;
    sub sinc {
        if ($_[0] eq 0.0) {
            return 1.0;
        } else {
            return sin($pi * $_[0]) / ($pi * $_[0]);
        }
    }
    sub lanczos {
        my $d = sqrt($_[0] * $_[0] + $_[1] * $_[1]);
        return sinc($d) / sinc($d / $_[2]);
    }
    my $r = 3.0; #radius (1 = 3 steps)
    my $k_size = ($r * 2.0 + 1.0) * ($r * 2.0 + 1.0);
    my $w_sum = \0.0;
    my $factor = 1.0 / ($r + 1.0);
    #kernel size = (r * 2 + 1) ^ 2
    printf("const float w_kernel[%i] = float[] (\n    ", $k_size);
    for (my $x = -$r; $x <= $r; $x++) {
        for (my $y = -$r; $y <= $r; $y++) {
            my $w = lanczos($x, $y, $r);
            printf("%f, ", $w);
            $w_sum += $w;
        }
    }
    printf("\n);\nconst vec2 w_pos[%i] = vec[](\n    ", $k_size);
    for (my $x = -$r; $x <= $r; $x++) {
        for (my $y = -$r; $y <= $r; $y++) {
            printf("vec2(%f, %f), ", $x * $factor, $y * $factor);
        }
    }
    printf("\n);\nconst float w_sum = %f;\n", $w_sum);
}
generate_lanczos;
