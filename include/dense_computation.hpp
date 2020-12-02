// 2020 Marcel Wagenländer

#ifndef ALZHEIMER_DENSE_COMPUTATION_H
#define ALZHEIMER_DENSE_COMPUTATION_H

#include "cuda_helper.hpp"
#include "tensors.hpp"


void mat_mat_add(CudaHelper *cuda_helper, Matrix<float> *mat_a, Matrix<float> *mat_b, Matrix<float> *result);

#endif//ALZHEIMER_DENSE_COMPUTATION_H
