// Copyright 2020 Marcel Wagenländer

#include "sage_linear.hpp"
#include "cuda_helper.hpp"


SageLinear::SageLinear(int in_features, int out_features, CudaHelper *helper) {
    cuda_helper_ = helper;

    num_in_features_ = in_features;
    num_out_features_ = out_features;
    linear_self_ = Linear(num_in_features_, num_out_features_, cuda_helper_);
    linear_neigh_= Linear(num_in_features_, num_out_features_, cuda_helper_);
}

matrix<float>* SageLinear::get_parameters() {
    matrix<float> *self_params = linear_self_.get_parameters();
    matrix<float> *neigh_params = linear_neigh_.get_parameters();
    matrix<float> *params = (matrix<float> *) malloc(4 * sizeof(matrix<float>));
    params[0] = self_params[0];
    params[1] = self_params[1];
    params[2] = neigh_params[0];
    params[3] = neigh_params[1];

    return params;
}

matrix<float> SageLinear::forward(matrix<float> features,
        matrix<float> aggr) {
    matrix<float> self_result = linear_self_.forward(features);
    matrix<float> neigh_result = linear_neigh_.forward(aggr);

    float *d_self;
    check_cuda(cudaMalloc((void **) &d_self,
                          self_result.rows * self_result.columns * sizeof(float)) );
    check_cuda(cudaMemcpy(d_self, self_result.values,
                          self_result.rows * self_result.columns * sizeof(float),
                          cudaMemcpyHostToDevice));

    float *d_neigh;
    check_cuda(cudaMalloc((void **) &d_neigh,
                          neigh_result.rows * neigh_result.columns * sizeof(float)));
    check_cuda(cudaMemcpy(d_neigh, neigh_result.values,
                          neigh_result.rows * neigh_result.columns * sizeof(float),
                          cudaMemcpyHostToDevice));

    float alpha = 1.0;
    check_cublas(cublasSaxpy(cuda_helper_->cublas_handle,
                             self_result.rows * self_result.columns, &alpha,
                             d_neigh, 1,
                             d_self, 1));

    check_cuda(cudaMemcpy(self_result.values, d_self,
                          self_result.rows * self_result.columns * sizeof(float),
                          cudaMemcpyDeviceToHost));

    check_cuda(cudaFree(d_self));
    check_cuda(cudaFree(d_neigh));

    return self_result;
}
