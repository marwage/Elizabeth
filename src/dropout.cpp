// Copyright 2020 Marcel Wagenländer

#include "dropout.hpp"
#include "cuda_helper.hpp"
#include "tensors.hpp"

#include <cmath>
#include <cstring>
#include <cuda_runtime.h>
#include <cudnn.h>
#include <iostream>


Dropout::Dropout(CudaHelper *helper) {
    cuda_helper_ = helper;
}

matrix<float> Dropout::forward(matrix<float> X) {
    float probability = 0.2f;
    check_cudnn(cudnnDropoutGetStatesSize(cuda_helper_->cudnn_handle, &state_size_));
    void *d_states;
    check_cuda(cudaMalloc(&d_states, state_size_));
    unsigned long long seed = rand();
    check_cudnn(cudnnCreateDropoutDescriptor(&dropout_desc_));
    check_cudnn(cudnnSetDropoutDescriptor(dropout_desc_,
                                          cuda_helper_->cudnn_handle, probability,
                                          d_states, state_size_, seed));

    matrix<float> Y;
    Y.rows = X.rows;
    Y.columns = X.columns;

    cudnnTensorDescriptor_t x_descr;
    check_cudnn(cudnnCreateTensorDescriptor(&x_descr));
    check_cudnn(cudnnSetTensor4dDescriptor(x_descr,
                                           CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT,
                                           1, 1, X.rows, X.columns));
    cudnnTensorDescriptor_t y_descr;
    check_cudnn(cudnnCreateTensorDescriptor(&y_descr));
    check_cudnn(cudnnSetTensor4dDescriptor(y_descr,
                                           CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT,
                                           1, 1, Y.rows, Y.columns));

    void *d_X, *d_Y;
    check_cuda(cudaMalloc(&d_X, X.rows * X.columns * sizeof(float)));
    check_cuda(cudaMemcpy(d_X, X.values, X.rows * X.columns * sizeof(float),
                          cudaMemcpyHostToDevice));
    check_cuda(cudaMalloc(&d_Y, Y.rows * Y.columns * sizeof(float)));

    void *d_reserve_space;
    check_cudnn(cudnnDropoutGetReserveSpaceSize(x_descr, &reserve_space_size_));
    check_cuda(cudaMalloc(&d_reserve_space, reserve_space_size_));
    check_cudnn(cudnnDropoutForward(cuda_helper_->cudnn_handle,
                                    dropout_desc_, x_descr, d_X,
                                    y_descr, d_Y,
                                    d_reserve_space, reserve_space_size_));

    Y.values = (float *) malloc(Y.rows * Y.columns * sizeof(float));
    check_cuda(cudaMemcpy(Y.values, d_Y, Y.rows * Y.columns * sizeof(float),
                          cudaMemcpyDeviceToHost));

    reserve_space_ = reinterpret_cast<void *>(malloc(reserve_space_size_));
    check_cuda(cudaMemcpy(reserve_space_, d_reserve_space,
                          reserve_space_size_,
                          cudaMemcpyDeviceToHost));

    states_ = reinterpret_cast<void *>(malloc(state_size_));
    check_cuda(cudaMemcpy(states_, d_states, state_size_,
                          cudaMemcpyDeviceToHost));

    check_cuda(cudaFree(d_states));
    check_cuda(cudaFree(d_reserve_space));
    check_cuda(cudaFree(d_X));
    check_cuda(cudaFree(d_Y));

    return Y;
}

matrix<float> Dropout::backward(matrix<float> in_gradients) {
    cudnnTensorDescriptor_t dy_desc;
    check_cudnn(cudnnCreateTensorDescriptor(&dy_desc));
    check_cudnn(cudnnSetTensor4dDescriptor(dy_desc,
                                           CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT,
                                           1, 1, in_gradients.rows, in_gradients.columns));
    float *d_dy;
    check_cuda(cudaMalloc(&d_dy, in_gradients.rows * in_gradients.columns * sizeof(float)));
    check_cuda(cudaMemcpy(d_dy, in_gradients.values,
                          in_gradients.rows * in_gradients.columns,
                          cudaMemcpyHostToDevice));
    cudnnTensorDescriptor_t dx_desc;
    check_cudnn(cudnnCreateTensorDescriptor(&dx_desc));
    check_cudnn(cudnnSetTensor4dDescriptor(dx_desc,
                                           CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT,
                                           1, 1, in_gradients.rows, in_gradients.columns));
    float *d_dx;
    check_cuda(cudaMalloc(&d_dx, in_gradients.rows * in_gradients.columns * sizeof(float)));

    void *d_reserve_space;
    check_cuda(cudaMalloc(&d_reserve_space, reserve_space_size_));
    check_cuda(cudaMemcpy(d_reserve_space, reserve_space_,
                          reserve_space_size_,
                          cudaMemcpyHostToDevice));

    // It is expected that reserveSpace was populated during a call to cudnnDropoutForward and has not been changed
    check_cudnn(cudnnDropoutBackward(cuda_helper_->cudnn_handle,
                                     dropout_desc_,
                                     dy_desc, d_dy,
                                     dx_desc, d_dx,
                                     d_reserve_space, reserve_space_size_));

    matrix<float> grad_input;
    grad_input.rows = in_gradients.rows;
    grad_input.columns = in_gradients.columns;
    grad_input.values = reinterpret_cast<float *>(malloc(grad_input.rows * grad_input.columns * sizeof(float)));
    check_cuda(cudaMemcpy(grad_input.values, d_dx,
                          grad_input.rows * grad_input.columns * sizeof(float),
                          cudaMemcpyDeviceToHost));

    //clean-up
    check_cuda(cudaFree(d_dy));
    check_cuda(cudaFree(d_dx));
    check_cuda(cudaFree(d_reserve_space));

    return grad_input;
}

DropoutChunked::DropoutChunked(CudaHelper *helper, int chunk_size) {
    cuda_helper_ = helper;
    chunk_size_ = chunk_size;
}

matrix<float> DropoutChunked::forward(matrix<float> X) {
    num_chunks_ = ceil((float) X.rows / (float) chunk_size_);

    dropout_layers_ = (Dropout *) malloc(num_chunks_ * sizeof(Dropout));
    for (int i = 0; i < num_chunks_; ++i) {
        dropout_layers_[i] = Dropout(cuda_helper_);
    }

    if (num_chunks_ * chunk_size_ > X.rows) {
        last_chunk_size_ = X.rows - (num_chunks_ - 1) * chunk_size_;
    } else {
        last_chunk_size_ = chunk_size_;
    }

    to_row_major(&X);

    matrix<float> Y;
    Y.rows = X.rows;
    Y.columns = X.columns;
    Y.values = reinterpret_cast<float *>(malloc(Y.rows * Y.columns * sizeof(float)));
    matrix<float> X_chunk;
    matrix<float> Y_chunk;

    for (int i = 0; i < num_chunks_; ++i) {
        std::cout << "i " << i << std::endl;
        if (i == (num_chunks_ - 1)) {
            X_chunk.rows = last_chunk_size_;
        } else {
            X_chunk.rows = chunk_size_;
        }
        X_chunk.columns = X.columns;
        X_chunk.values = &X.values[i * chunk_size_];

        Y_chunk = dropout_layers_[i].forward(X_chunk);

        std::memcpy(&Y.values[i * chunk_size_], Y_chunk.values, Y_chunk.rows * Y_chunk.columns * sizeof(float));
    }

    to_column_major(&Y);

    return Y;
}
