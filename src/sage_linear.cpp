// Copyright 2020 Marcel Wagenländer

#include "sage_linear.hpp"

#include <assert.h>
#include <cmath>
#include <string>

#include <iostream> // DEBUGGING


SageLinear::SageLinear() {}

SageLinear::SageLinear(CudaHelper *helper, long in_features, long out_features, long num_nodes) {
    cuda_helper_ = helper;

    num_in_features_ = in_features;
    num_out_features_ = out_features;
    linear_self_ = Linear(cuda_helper_, num_in_features_, num_out_features_, num_nodes);
    linear_neigh_ = Linear(cuda_helper_, num_in_features_, num_out_features_, num_nodes);
}

matrix<float>** SageLinear::get_parameters() {
    matrix<float> **self_params = linear_self_.get_parameters();
    matrix<float> **neigh_params = linear_neigh_.get_parameters();
    matrix<float> **params = new matrix<float>*[4];
    params[0] = self_params[0];
    params[1] = self_params[1];
    params[2] = neigh_params[0];
    params[3] = neigh_params[1];

    return params;
}

// assume number of parameters is 4
void SageLinear::set_parameters(matrix<float> **parameters) {
    linear_self_.set_parameters(parameters);
    linear_neigh_.set_parameters(&parameters[2]);
}

matrix<float>** SageLinear::get_gradients() {
    matrix<float> **self_grads = linear_self_.get_gradients();
    matrix<float> **neigh_grads = linear_neigh_.get_gradients();
    matrix<float> **grads = new matrix<float>*[4];
    grads[0] = self_grads[0];
    grads[1] = self_grads[1];
    grads[2] = neigh_grads[0];
    grads[3] = neigh_grads[1];

    return grads;
}

void SageLinear::set_gradients(matrix<float> **grads) {
    linear_self_.set_gradients(grads);
    linear_neigh_.set_gradients(&grads[2]);
}

matrix<float>* SageLinear::forward(matrix<float> *features, matrix<float> *aggr) {
    matrix<float> *self_result = linear_self_.forward(features);
    matrix<float> *neigh_result = linear_neigh_.forward(aggr);

    float *d_self;
    check_cuda(cudaMalloc((void **) &d_self,
                          self_result->rows * self_result->columns * sizeof(float)));
    check_cuda(cudaMemcpy(d_self, self_result->values,
                          self_result->rows * self_result->columns * sizeof(float),
                          cudaMemcpyHostToDevice));

    float *d_neigh;
    check_cuda(cudaMalloc((void **) &d_neigh,
                          neigh_result->rows * neigh_result->columns * sizeof(float)));
    check_cuda(cudaMemcpy(d_neigh, neigh_result->values,
                          neigh_result->rows * neigh_result->columns * sizeof(float),
                          cudaMemcpyHostToDevice));

    float alpha = 1.0;
    check_cublas(cublasSaxpy(cuda_helper_->cublas_handle,
                             self_result->rows * self_result->columns, &alpha,
                             d_neigh, 1,
                             d_self, 1));

    check_cuda(cudaMemcpy(self_result->values, d_self,
                          self_result->rows * self_result->columns * sizeof(float),
                          cudaMemcpyDeviceToHost));

    check_cuda(cudaFree(d_self));
    check_cuda(cudaFree(d_neigh));

    return self_result;
}

SageLinearGradients* SageLinear::backward(matrix<float> *in_gradients) {
    input_gradients_.self_grads = linear_self_.backward(in_gradients);
    input_gradients_.neigh_grads = linear_neigh_.backward(in_gradients);

    return &input_gradients_;
}

void SageLinear::update_weights(matrix<float> *gradients) {
    linear_self_.update_weights(gradients);
    linear_neigh_.update_weights(&gradients[2]);
}

SageLinearChunked::SageLinearChunked(CudaHelper *helper, long num_in_features, long num_out_features, long chunk_size, long num_nodes) {
    cuda_helper_ = helper;
    chunk_size_ = chunk_size;
    num_in_features_ = num_in_features;
    num_out_features_ = num_out_features;

    num_chunks_ = ceil((float) num_nodes / (float) chunk_size_);
    if (num_chunks_ * chunk_size_ > num_nodes) {
        last_chunk_size_ = num_nodes - (num_chunks_ - 1) * chunk_size_;
    } else {
        last_chunk_size_ = chunk_size_;
    }

    sage_linear_layers_ = std::vector<SageLinear>(num_chunks_);
    features_chunks_ = std::vector<matrix<float>>(num_chunks_);
    aggr_chunks_ = std::vector<matrix<float>>(num_chunks_);
    in_gradients_chunks_ = std::vector<matrix<float>>(num_chunks_);
    for (int i = 0; i < num_chunks_; ++i) {
        features_chunks_[i].columns = num_in_features;
        aggr_chunks_[i].columns = num_in_features;
        in_gradients_chunks_[i].columns = num_out_features;
        if (i == (num_chunks_ - 1)) {
            sage_linear_layers_[i] = SageLinear(cuda_helper_, num_in_features_, num_out_features_, last_chunk_size_);
            features_chunks_[i].rows = last_chunk_size_;
            aggr_chunks_[i].rows = last_chunk_size_;
            in_gradients_chunks_[i].rows = last_chunk_size_;
        } else {
            sage_linear_layers_[i] = SageLinear(cuda_helper_, num_in_features_, num_out_features_, chunk_size_);
            features_chunks_[i].rows = chunk_size_;
            aggr_chunks_[i].rows = chunk_size_;
            in_gradients_chunks_[i].rows = chunk_size_;
        }

        features_chunks_[i].values = new float[features_chunks_[i].rows * features_chunks_[i].columns];
        aggr_chunks_[i].values = new float[aggr_chunks_[i].rows * aggr_chunks_[i].columns];
        in_gradients_chunks_[i].values = new float[in_gradients_chunks_[i].rows * in_gradients_chunks_[i].columns];
    }

    if (num_chunks_ > 1) {
        matrix<float> **params = sage_linear_layers_[0].get_parameters();
        for (int i = 1; i < num_chunks_; ++i) {
            sage_linear_layers_[i].set_parameters(params);
        }
    }

    y_ = new_float_matrix(num_nodes, num_out_features, true);

    self_gradients_ = new_float_matrix(num_nodes, num_in_features, true);
    neighbourhood_gradients_ = new_float_matrix(num_nodes, num_in_features, true);
    input_gradients_.self_grads = &self_gradients_;
    input_gradients_.neigh_grads = &neighbourhood_gradients_;
}

matrix<float>* SageLinearChunked::forward(matrix<float> *features, matrix<float> *aggr) {
    to_row_major_inplace(features);
    to_row_major_inplace(aggr);

    matrix<float> *y_chunk;
    for (int i = 0; i < num_chunks_; ++i) {
        features_chunks_[i].row_major = features->row_major;
        aggr_chunks_[i].row_major = aggr->row_major;
        std::memcpy(features_chunks_[i].values, &features->values[i * chunk_size_ * features->columns],
                    features_chunks_[i].rows * features_chunks_[i].columns * sizeof(float));
        std::memcpy(aggr_chunks_[i].values, &aggr->values[i * chunk_size_ * aggr->columns],
                    aggr_chunks_[i].rows * aggr_chunks_[i].columns * sizeof(float));

        y_chunk = sage_linear_layers_[i].forward(&features_chunks_[i], &aggr_chunks_[i]);

        to_row_major_inplace(y_chunk);
        std::memcpy(&y_.values[i * chunk_size_ * y_chunk->columns], y_chunk->values,
                    y_chunk->rows * y_chunk->columns * sizeof(float));
    }

    y_.row_major = true;

    return &y_;
}

SageLinearGradients* SageLinearChunked::backward(matrix<float> *in_gradients) {
    to_row_major_inplace(in_gradients);

    SageLinearGradients *gradients_chunk;
    for (int i = 0; i < num_chunks_; ++i) {
        in_gradients_chunks_[i].row_major = in_gradients->row_major;
        std::memcpy(in_gradients_chunks_[i].values, &in_gradients->values[i * chunk_size_ * in_gradients->columns],
                    in_gradients_chunks_[i].rows * in_gradients_chunks_[i].columns * sizeof(float));

        gradients_chunk = sage_linear_layers_[i].backward(&in_gradients_chunks_[i]);

        to_row_major_inplace(gradients_chunk->self_grads);
        to_row_major_inplace(gradients_chunk->neigh_grads);
        std::memcpy(&input_gradients_.self_grads->values[i * chunk_size_ * num_in_features_],
                    gradients_chunk->self_grads->values,
                    gradients_chunk->self_grads->rows * gradients_chunk->self_grads->columns * sizeof(float));
        std::memcpy(&input_gradients_.neigh_grads->values[i * chunk_size_ * num_in_features_],
                    gradients_chunk->neigh_grads->values,
                    gradients_chunk->neigh_grads->rows * gradients_chunk->neigh_grads->columns * sizeof(float));
    }

    // add gradients of all layers
    if (num_chunks_ > 1) {
        matrix<float> **gradients = sage_linear_layers_[0].get_gradients();
        int num_parameters = 4;
        float alpha = 1;
        float *d_sum;
        float *d_gradient;
        for (int j = 0; j < num_parameters; ++j) {
            check_cuda(cudaMalloc((void **) &d_sum,
                                  gradients[j]->rows * gradients[j]->columns * sizeof(float)));
            check_cuda(cudaMemcpy(d_sum, gradients[j]->values,
                                  gradients[j]->rows * gradients[j]->columns * sizeof(float),
                                  cudaMemcpyHostToDevice));
            check_cuda(cudaMalloc((void **) &d_gradient,
                                  gradients[j]->rows * gradients[j]->columns * sizeof(float)));

            for (int i = 1; i < num_chunks_; ++i) {
                matrix<float> **gradients_i = sage_linear_layers_[i].get_gradients();
                check_cuda(cudaMemcpy(d_gradient, gradients_i[j]->values,
                                      gradients_i[j]->rows * gradients_i[j]->columns * sizeof(float),
                                      cudaMemcpyHostToDevice));
                check_cublas(cublasSaxpy(cuda_helper_->cublas_handle, gradients[j]->rows * gradients[j]->columns,
                                         &alpha, d_gradient, 1, d_sum, 1));
            }

            check_cuda(cudaMemcpy(gradients[j]->values, d_sum,
                                  gradients[j]->rows * gradients[j]->columns * sizeof(float),
                                  cudaMemcpyDeviceToHost));

            cudaFree(d_sum);
            cudaFree(d_gradient);
        }

        for (int i = 1; i < num_chunks_; ++i) {
            sage_linear_layers_[i].set_gradients(sage_linear_layers_[0].get_gradients());
        }
    }

    input_gradients_.neigh_grads->row_major = true;
    input_gradients_.neigh_grads->row_major = true;

    return &input_gradients_;
}

matrix<float>** SageLinearChunked::get_parameters() {
    return sage_linear_layers_[0].get_parameters();
}

void SageLinearChunked::set_parameters(matrix<float> **parameters) {
    for (int i = 0; i < num_chunks_; ++i) {
        sage_linear_layers_[i].set_parameters(parameters);
    }
}

matrix<float>** SageLinearChunked::get_gradients() {
    return sage_linear_layers_[0].get_gradients();
}

void SageLinearChunked::update_weights(matrix<float> *gradients) {
    for (int i = 0; i < num_chunks_; ++i) {
        sage_linear_layers_[i].update_weights(gradients);
    }
}

std::vector<SageLinear>* SageLinearChunked::get_layers() {
    return &sage_linear_layers_;
}
