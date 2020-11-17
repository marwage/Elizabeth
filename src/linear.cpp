// Copyright 2020 Marcel Wagenländer

#include <cuda_runtime.h>
#include <random>

#include "cuda_helper.hpp"
#include "linear.hpp"
#include "tensors.hpp"

#include <assert.h>// DEBUGGING


Linear::Linear() {}

Linear::Linear(int in_features, int out_features, CudaHelper *helper) {
    cuda_helper_ = helper;

    num_in_features_ = in_features;
    num_out_features_ = out_features;

    weight_.rows = num_in_features_;
    weight_.columns = num_out_features_;
    bias_.rows = num_out_features_;
    bias_.columns = 1;

    grad_weight_.rows = weight_.rows;
    grad_weight_.columns = weight_.columns;
    grad_weight_.values = reinterpret_cast<float *>(malloc(grad_weight_.rows * grad_weight_.columns * sizeof(float)));
    grad_bias_.rows = bias_.rows;
    grad_bias_.columns = bias_.columns;
    grad_bias_.values = reinterpret_cast<float *>(malloc(grad_bias_.rows * grad_bias_.columns * sizeof(float)));

    Linear::init_weight_bias();
}

void Linear::init_weight_bias() {
    weight_.values = reinterpret_cast<float *>(
            malloc(weight_.rows * weight_.columns * sizeof(float)));
    bias_.values = reinterpret_cast<float *>(
            malloc(bias_.rows * bias_.columns * sizeof(float)));

    double k = 1.0 / static_cast<double>(num_out_features_);
    k = sqrt(k);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distr(-k, k);
    for (int i = 0; i < weight_.rows * weight_.columns; ++i) {
        weight_.values[i] = distr(gen);
    }
    for (int i = 0; i < bias_.rows * bias_.columns; ++i) {
        bias_.values[i] = distr(gen);
    }
}

matrix<float> *Linear::get_parameters() {
    matrix<float> *parameters = (matrix<float> *) malloc(2 * sizeof(matrix<float>));
    parameters[0] = weight_;
    parameters[1] = bias_;

    return parameters;
}

void Linear::set_parameters(matrix<float> *parameters) {
    weight_ = parameters[0];
    bias_ = parameters[1];
}

matrix<float> *Linear::get_gradients() {
    matrix<float> *grads = (matrix<float> *) malloc(2 * sizeof(matrix<float>));
    grads[0] = grad_weight_;
    grads[1] = grad_bias_;

    return grads;
}

void Linear::set_gradients(matrix<float> *grads) {
    grad_weight_ = grads[0];
    grad_bias_ = grads[1];
}

matrix<float> Linear::expand_bias(int num_rows) {
    matrix<float> bias_expanded;
    bias_expanded.rows = num_rows;
    bias_expanded.columns = bias_.rows;
    bias_expanded.values = reinterpret_cast<float *>(
            malloc(bias_expanded.rows * bias_expanded.columns * sizeof(float)));

    for (int i = 0; i < bias_expanded.columns; ++i) {
        for (int j = 0; j < bias_expanded.rows; ++j) {
            bias_expanded.values[i * bias_expanded.rows + j] = bias_.values[i];
        }
    }

    return bias_expanded;
}

// assume X is column-major
matrix<float> Linear::forward(matrix<float> X) {
    if (X.rows < 1) {
        throw "Input to Linear::forward has a non-positive number of rows";
    }
    x_ = X;

    float *d_X, *d_weight, *d_bias;
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&d_X),
                          X.rows * X.columns * sizeof(float)));
    check_cuda(cudaMemcpy(d_X, X.values,
                          X.rows * X.columns * sizeof(float),
                          cudaMemcpyHostToDevice));

    check_cuda(cudaMalloc(&d_weight,
                          weight_.rows * weight_.columns * sizeof(float)));
    check_cuda(cudaMemcpy(d_weight, weight_.values,
                          weight_.rows * weight_.columns * sizeof(float),
                          cudaMemcpyHostToDevice));

    matrix<float> bias_expanded = Linear::expand_bias(X.rows);
    check_cuda(cudaMalloc(&d_bias,
                          bias_expanded.rows * bias_expanded.columns * sizeof(float)));
    check_cuda(cudaMemcpy(d_bias, bias_expanded.values,
                          bias_expanded.rows * bias_expanded.columns * sizeof(float),
                          cudaMemcpyHostToDevice));

    float alpha = 1.0;
    float beta = 1.0;
    check_cublas(cublasSgemm(cuda_helper_->cublas_handle,// PyTorch uses GEMM too
                             CUBLAS_OP_N, CUBLAS_OP_N,
                             X.rows, weight_.columns, X.columns,
                             &alpha,
                             d_X, X.rows,
                             d_weight, weight_.rows,
                             &beta,
                             d_bias, X.rows));

    // get result of linear
    matrix<float> result;
    result.rows = X.rows;
    result.columns = weight_.columns;
    result.values = reinterpret_cast<float *>(
            malloc(result.rows * result.columns * sizeof(float)));
    check_cuda(cudaMemcpy(result.values, d_bias,
                          result.rows * result.columns * sizeof(float),
                          cudaMemcpyDeviceToHost));

    // free GPU memory
    check_cuda(cudaFree(d_X));
    check_cuda(cudaFree(d_weight));
    check_cuda(cudaFree(d_bias));

    // free CPU memory
    free(bias_expanded.values);

    return result;
}

matrix<float> Linear::backward(matrix<float> in_gradients) {
    float alpha = 1.0;
    float beta = 0.0;

    // gradients of bias
    float *d_g;
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&d_g),
                          in_gradients.rows * in_gradients.columns * sizeof(float)));
    check_cuda(cudaMemcpy(d_g, in_gradients.values,
                          in_gradients.rows * in_gradients.columns * sizeof(float),
                          cudaMemcpyHostToDevice));

    float *d_ones;
    float *ones = reinterpret_cast<float *>(malloc(in_gradients.rows * sizeof(float)));
    for (int i = 0; i < in_gradients.rows; ++i) {
        ones[i] = 1.0;
    }
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&d_ones),
                          in_gradients.rows * sizeof(float)));
    check_cuda(cudaMemcpy(d_ones, ones,
                          in_gradients.rows * sizeof(float),
                          cudaMemcpyHostToDevice));

    float *d_db;
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&d_db),
                          in_gradients.columns * sizeof(float)));


    check_cublas(cublasSgemv(cuda_helper_->cublas_handle,
                             CUBLAS_OP_T,
                             in_gradients.rows, in_gradients.columns,
                             &alpha, d_g, in_gradients.rows,
                             d_ones, 1,
                             &beta, d_db, 1));

    check_cuda(cudaMemcpy(grad_bias_.values, d_db,
                          grad_bias_.rows * grad_bias_.columns * sizeof(float),
                          cudaMemcpyDeviceToHost));

    check_cuda(cudaFree(d_ones));
    check_cuda(cudaFree(d_db));

    // DEBUGGING
    //    assert(grad_bias_.rows == in_gradients.columns);
    //    assert(grad_bias_.columns == 1);
    //    for (int i = 0; i < in_gradients.columns; ++i) {
    //        grad_bias_.values[i] = 0.0;
    //        for (int j = 0; j < in_gradients.rows; ++j) {
    //            grad_bias_.values[i] += in_gradients.values[i * in_gradients.rows + j];
    //        }
    //    }

    // gradient of weight
    // gradients_input = in_gradients * weight.T
    matrix<float> grad_input;
    grad_input.rows = in_gradients.rows;
    grad_input.columns = weight_.rows;
    grad_input.values = reinterpret_cast<float *>(malloc(grad_input.rows * grad_input.columns * sizeof(float)));

    float *d_weight;
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&d_weight),
                          weight_.rows * weight_.columns * sizeof(float)));
    check_cuda(cudaMemcpy(d_weight, weight_.values,
                          weight_.rows * weight_.columns * sizeof(float),
                          cudaMemcpyHostToDevice));

    float *d_dinput;
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&d_dinput),
                          grad_input.rows * grad_input.columns * sizeof(float)));

    check_cublas(cublasSgemm(cuda_helper_->cublas_handle,
                             CUBLAS_OP_N, CUBLAS_OP_T,
                             in_gradients.rows, weight_.rows, in_gradients.columns,
                             &alpha,
                             d_g, in_gradients.rows,
                             d_weight, weight_.rows,
                             &beta,
                             d_dinput, grad_input.rows));

    check_cuda(cudaMemcpy(grad_input.values, d_dinput,
                          grad_input.rows * grad_input.columns * sizeof(float),
                          cudaMemcpyDeviceToHost));

    // dWeight = input.T * in_gradients
    float *d_input;
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&d_input),
                          x_.rows * x_.columns * sizeof(float)));
    check_cuda(cudaMemcpy(d_input, x_.values,
                          x_.rows * x_.columns * sizeof(float),
                          cudaMemcpyHostToDevice));

    float *d_dweight;
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&d_dweight),
                          grad_weight_.rows * grad_weight_.columns * sizeof(float)));

    check_cublas(cublasSgemm(cuda_helper_->cublas_handle,
                             CUBLAS_OP_T, CUBLAS_OP_N,
                             x_.columns, in_gradients.columns, x_.rows,
                             &alpha,
                             d_input, x_.rows,
                             d_g, in_gradients.rows,
                             &beta,
                             d_dweight, grad_weight_.rows));

    check_cuda(cudaMemcpy(grad_weight_.values, d_dweight,
                          grad_weight_.rows * grad_weight_.columns * sizeof(float),
                          cudaMemcpyDeviceToHost));

    check_cuda(cudaFree(d_g));
    check_cuda(cudaFree(d_weight));
    check_cuda(cudaFree(d_dweight));
    check_cuda(cudaFree(d_input));
    check_cuda(cudaFree(d_dinput));

    return grad_input;
}

void Linear::update_weights(matrix<float> *gradients) {
    grad_weight_ = gradients[0];
    grad_bias_ = gradients[1];

    float *d_grads;
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&d_grads),
                          grad_weight_.rows * grad_weight_.columns * sizeof(float)));
    check_cuda(cudaMemcpy(d_grads, grad_weight_.values,
                          grad_weight_.rows * grad_weight_.columns * sizeof(float),
                          cudaMemcpyHostToDevice));

    float *d_weight;
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&d_weight),
                          weight_.rows * weight_.columns * sizeof(float)));
    check_cuda(cudaMemcpy(d_weight, weight_.values,
                          weight_.rows * weight_.columns * sizeof(float),
                          cudaMemcpyHostToDevice));

    float alpha = -1.0;
    check_cublas(cublasSaxpy(cuda_helper_->cublas_handle,
                             weight_.rows * weight_.columns,
                             &alpha, d_grads, 1,
                             d_weight, 1));

    check_cuda(cudaMemcpy(weight_.values, d_weight,
                          weight_.rows * weight_.columns * sizeof(float),
                          cudaMemcpyDeviceToHost));

    // clean-up
    check_cuda(cudaFree(d_grads));
    check_cuda(cudaFree(d_weight));

    check_cuda(cudaMalloc(reinterpret_cast<void **>(&d_grads),
                          grad_bias_.rows * grad_bias_.columns * sizeof(float)));
    check_cuda(cudaMemcpy(d_grads, grad_bias_.values,
                          grad_bias_.rows * grad_bias_.columns * sizeof(float),
                          cudaMemcpyHostToDevice));

    float *d_bias;
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&d_bias),
                          bias_.rows * bias_.columns * sizeof(float)));
    check_cuda(cudaMemcpy(d_bias, bias_.values,
                          bias_.rows * bias_.columns * sizeof(float),
                          cudaMemcpyHostToDevice));

    check_cublas(cublasSaxpy(cuda_helper_->cublas_handle,
                             bias_.rows * bias_.columns,
                             &alpha, d_grads, 1,
                             d_bias, 1));

    check_cuda(cudaMemcpy(bias_.values, d_bias,
                          bias_.rows * bias_.columns * sizeof(float),
                          cudaMemcpyDeviceToHost));

    // clean-up
    check_cuda(cudaFree(d_grads));
    check_cuda(cudaFree(d_bias));
}
