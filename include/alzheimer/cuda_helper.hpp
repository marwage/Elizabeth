// Copyright 2020 Marcel Wagenländer

#ifndef CUDA_HELPER_H
#define CUDA_HELPER_H

#include <cuda_runtime.h>
#include "cusparse.h"
#include <cudnn.h>
#include <cublas_v2.h>

class CudaHelper {
private:
    cublasStatus_t cublas_status;
    cudnnStatus_t cudnn_status;
    cusparseStatus_t cusparse_status;
public:
    cublasHandle_t cublas_handle;
    cudnnHandle_t cudnn_handle;
    cusparseHandle_t cusparse_handle;

    CudaHelper();
    void destroy_handles();
};


void check_cuda(cudaError_t status);

void check_cusparse(cusparseStatus_t status);

void check_cudnn(cudnnStatus_t status);

void check_cublas(cublasStatus_t status);

#endif
