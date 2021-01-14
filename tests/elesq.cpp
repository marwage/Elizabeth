// Copyright 2020 Marcel Wagenländer

#include "elesq.h"
#include "cuda_helper.hpp"

#include "catch2/catch.hpp"
#include <cmath>


int test_elesq(int num_elements) {
    float *x = new float[num_elements];
    float *x_result = new float[num_elements];
    float *d_x_result = new float[num_elements];
    float *d_x;

    for (int i = 0; i < num_elements; ++i) {
        x[i] = rand();
    }

    for (int i = 0; i < num_elements; ++i) {
        x_result[i] = x[i] * x[i];
    }

    check_cuda(cudaMalloc(&d_x, num_elements * sizeof(float)));
    check_cuda(cudaMemcpy(d_x, x, num_elements * sizeof(float), cudaMemcpyHostToDevice));

    ele_squared(d_x, num_elements);

    check_cuda(cudaMemcpy(d_x_result, d_x, num_elements * sizeof(float), cudaMemcpyDeviceToHost));

    int equal = 1;
    for (int i = 0; i < num_elements; ++i) {
        if (x_result[i] != d_x_result[i]) {
            equal = 0;
        }
    }

    return equal;
}


TEST_CASE("Element-wise squared", "[elesq]") {
    CHECK(test_elesq(1e3));
    CHECK(test_elesq(1e4));
    CHECK(test_elesq(1e5));
    CHECK(test_elesq(1e6));
}