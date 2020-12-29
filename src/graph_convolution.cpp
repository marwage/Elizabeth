// Copyright 2020 Marcel Wagenländer

#include "cuda_helper.hpp"
#include "divmv.h"
#include "feature_aggregation.hpp"
#include "sparse_computation.hpp"

#include <cmath>
#include <string>


GraphConvolution::GraphConvolution() {}

GraphConvolution::GraphConvolution(CudaHelper *helper, SparseMatrix<float> *adjacency, std::string reduction,
                                   long num_nodes, long num_features) {
    set(helper, adjacency, reduction, num_nodes, num_features);
}

void GraphConvolution::set(CudaHelper *helper, SparseMatrix<float> *adjacency, std::string reduction,
                           long num_nodes, long num_features) {
    name_ = "feature-aggregation";
    cuda_helper_ = helper;
    adjacency_ = adjacency;
    reduction_ = reduction;
    if (reduction_.compare("mean") == 0) {
        mean_ = true;
    } else if (reduction_.compare("sum") == 0) {
        mean_ = false;
    } else {
        throw "Reduction not supported";
    }

    y_.set(num_nodes, num_features, false);
    gradients_.set(num_nodes, num_features, false);

    if (mean_) {
        sum_.set(num_nodes, 1, false);
        sp_mat_sum_rows(cuda_helper_, adjacency_, &sum_);
    }
}

Matrix<float> *GraphConvolution::forward(Matrix<float> *x) {
    to_column_major_inplace(x);

    sp_mat_mat_multi(cuda_helper_, adjacency_, x, &y_, false);

    // apply mean
    if (mean_) {
        float *d_y;
        check_cuda(cudaMalloc((void **) &d_y, y_.size_ * sizeof(float)));
        check_cuda(cudaMemcpy(d_y, y_.values_, y_.size_ * sizeof(float),
                              cudaMemcpyHostToDevice));

        float *d_sum;
        check_cuda(cudaMalloc(&d_sum, sum_.size_ * sizeof(float)));
        check_cuda(cudaMemcpy(d_sum, sum_.values_, sum_.size_ * sizeof(float),
                              cudaMemcpyHostToDevice));

        div_mat_vec(d_y, d_sum, y_.num_rows_, y_.num_columns_);

        check_cuda(cudaMemcpy(y_.values_, d_y,
                              y_.size_ * sizeof(float),
                              cudaMemcpyDeviceToHost));

        check_cuda(cudaFree(d_y));
        check_cuda(cudaFree(d_sum));
    }

    y_.is_row_major_ = false;

    return &y_;
}

Matrix<float> *GraphConvolution::backward(Matrix<float> *in_gradients) {
    to_column_major_inplace(in_gradients);

    if (mean_) {
        float *d_g;
        check_cuda(cudaMalloc(&d_g, in_gradients->size_ * sizeof(float)));
        check_cuda(cudaMemcpy(d_g, in_gradients->values_,
                              in_gradients->size_ * sizeof(float), cudaMemcpyHostToDevice));

        float *d_sum;
        check_cuda(cudaMalloc(&d_sum, sum_.size_ * sizeof(float)));
        check_cuda(cudaMemcpy(d_sum, sum_.values_,
                              sum_.size_ * sizeof(float), cudaMemcpyHostToDevice));

        div_mat_vec(d_g, d_sum, in_gradients->num_rows_, in_gradients->num_columns_);

        check_cuda(cudaMemcpy(gradients_.values_, d_g,
                              gradients_.size_ * sizeof(float), cudaMemcpyDeviceToHost));

        check_cuda(cudaFree(d_g));
        check_cuda(cudaFree(d_sum));
    }

    // gradients_ = adjacency.T * in_gradients
    sp_mat_mat_multi(cuda_helper_, adjacency_, &gradients_, &gradients_, false);

    return &gradients_;
}

// CHUNKED --- CHUNKED --- CHUNKED

GraphConvChunked::GraphConvChunked() {}

GraphConvChunked::GraphConvChunked(CudaHelper *helper, SparseMatrix<float> *adjacency, std::string reduction,
                                   long num_features, long chunk_size, long num_nodes) {
    set(helper, adjacency, reduction, num_features, chunk_size, num_nodes);
}

void GraphConvChunked::set(CudaHelper *helper, SparseMatrix<float> *adjacency, std::string reduction,
                           long num_features, long chunk_size, long num_nodes) {
    name_ = "feature-aggregation_chunked";
    cuda_helper_ = helper;
    chunk_size_ = chunk_size;
    adjacency_ = adjacency;
    if (reduction.compare("mean") == 0) {
        mean_ = true;
    } else if (reduction.compare("sum") == 0) {
        mean_ = false;
    } else {
        throw "Reduction not supported";
    }

    num_chunks_ = ceil((float) num_nodes / (float) chunk_size_);
    if (num_chunks_ * chunk_size_ > num_nodes) {
        last_chunk_size_ = num_nodes - (num_chunks_ - 1) * chunk_size_;
    } else {
        last_chunk_size_ = chunk_size_;
    }

    adjacencies_ = std::vector<SparseMatrix<float>>(num_chunks_ * num_chunks_);
    long current_end_row;// end row is included [start_row, end_row] not [start_row, end_row)
    for (int i = 0; i < num_chunks_; ++i) {
        if (i == num_chunks_ - 1) {
            current_end_row = i * chunk_size + last_chunk_size_ - 1;
        } else {
            current_end_row = (i + 1) * chunk_size - 1;
        }

        // chunk by row
        SparseMatrix<float> adjacency_chunk;
        get_rows(&adjacency_chunk, adjacency, i * chunk_size, current_end_row);
        // transpose
        transpose_csr_matrix_cpu(&adjacency_chunk);
        // chunk by row (would be by column without transpose
        for (int j = 0; j < num_chunks_; ++j) {
            if (j == num_chunks_ - 1) {
                current_end_row = j * chunk_size + last_chunk_size_ - 1;
            } else {
                current_end_row = (j + 1) * chunk_size - 1;
            }

            get_rows(&adjacencies_[i * num_chunks_ + j], &adjacency_chunk, j * chunk_size, current_end_row);
            // transpose
            transpose_csr_matrix_cpu(&adjacencies_[i * num_chunks_ + j]);
        }
    }

    y_ = std::vector<Matrix<float>>(num_chunks_);
    gradients_ = std::vector<Matrix<float>>(num_chunks_);
    long current_chunk_size = chunk_size_;
    for (int i = 0; i < num_chunks_; ++i) {
        if (i == num_chunks_ - 1) {
            current_chunk_size = last_chunk_size_;
        }

        y_.at(i).set(current_chunk_size, num_features, false);
        gradients_.at(i).set(current_chunk_size, num_features, false);
    }

    sum_.set(num_nodes, 1, true);
    if (mean_) {
        sp_mat_sum_rows(adjacency_, &sum_);
    }
}

std::vector<Matrix<float>> *GraphConvChunked::forward(std::vector<Matrix<float>> *x) {
    for (int i = 0; i < num_chunks_; ++i) {
        to_column_major_inplace(&x->at(i));
    }

    float *d_y;
    check_cuda(cudaMalloc(&d_y, y_.at(0).size_ * sizeof(float)));

    float *d_sum;
    if (mean_) {
        check_cuda(cudaMalloc(&d_sum, y_.at(0).num_rows_ * sizeof(float)));
    }

    float *d_x;
    check_cuda(cudaMalloc(&d_x, x->at(0).size_ * sizeof(float)));

    // row chunk
    for (int i = 0; i < num_chunks_; ++i) {
        // column chunk of row chunk
        check_cuda(cudaMemset(d_y, 0, y_.at(i).size_ * sizeof(float)));

        for (int j = 0; j < num_chunks_; ++j) {
            SparseMatrixCuda<float> d_adj_i;
            SparseMatrix<float> *adj = &adjacencies_[i * num_chunks_ + j];
            if (adj->nnz_ > 0) {
                malloc_memcpy_sp_mat(&d_adj_i, adj);

                check_cuda(cudaMemcpy(d_x, x->at(j).values_, x->at(j).size_ * sizeof(float), cudaMemcpyHostToDevice));

                sp_mat_mat_multi_cuda(cuda_helper_, &d_adj_i, d_x, d_y, x->at(j).num_columns_, true);
            }
        }

        if (mean_) {
            check_cuda(cudaMemcpy(d_sum, &sum_.values_[i * chunk_size_], y_.at(i).num_rows_ * sizeof(float),
                                  cudaMemcpyHostToDevice));

            div_mat_vec(d_y, d_sum, y_.at(i).num_rows_, y_.at(i).num_columns_);
        }

        check_cuda(cudaMemcpy(y_.at(i).values_, d_y, y_.at(i).size_ * sizeof(float),
                              cudaMemcpyDeviceToHost));
    }

    // free GPU memory
    if (mean_) {
        check_cuda(cudaFree(d_sum));
    }
    check_cuda(cudaFree(d_y));
    check_cuda(cudaFree(d_x));

    return &y_;
}

std::vector<Matrix<float>> *GraphConvChunked::backward(std::vector<Matrix<float>> *incoming_gradients) {
    for (int i = 0; i < num_chunks_; ++i) {
        to_column_major_inplace(&incoming_gradients->at(i));
    }

    float *d_gradients;
    check_cuda(cudaMalloc(&d_gradients, gradients_.at(0).size_ * sizeof(float)));

    float *d_sum;
    if (mean_) {
        check_cuda(cudaMalloc(&d_sum, incoming_gradients->at(0).num_rows_ * sizeof(float)));
    }

    float *d_incoming_gradients;
    check_cuda(cudaMalloc(&d_incoming_gradients, incoming_gradients->at(0).size_ * sizeof(float)));

    // row chunk
    for (int i = 0; i < num_chunks_; ++i) {
        // column chunk of row chunk
        check_cuda(cudaMemset(d_gradients, 0, gradients_.at(i).size_ * sizeof(float)));

        for (int j = 0; j < num_chunks_; ++j) {
            SparseMatrixCuda<float> d_adj_i;
            SparseMatrix<float> *adj = &adjacencies_[i * num_chunks_ + j];
            if (adj->nnz_ > 0) {
                malloc_memcpy_sp_mat(&d_adj_i, &adjacencies_[i * num_chunks_ + j]);

                check_cuda(cudaMemcpy(d_incoming_gradients, incoming_gradients->at(j).values_, incoming_gradients->at(j).size_ * sizeof(float), cudaMemcpyHostToDevice));

                if (mean_) {
                    check_cuda(cudaMemcpy(d_sum, &sum_.values_[j * chunk_size_], incoming_gradients->at(j).num_rows_ * sizeof(float),
                                          cudaMemcpyHostToDevice));

                    div_mat_vec(d_incoming_gradients, d_sum, incoming_gradients->at(j).num_rows_, incoming_gradients->at(j).num_columns_);
                }

                sp_mat_mat_multi_cuda(cuda_helper_, &d_adj_i, d_incoming_gradients, d_gradients, incoming_gradients->at(j).num_columns_, true);
            }
        }

        check_cuda(cudaMemcpy(gradients_.at(i).values_, d_gradients, gradients_.at(i).size_ * sizeof(float),
                              cudaMemcpyDeviceToHost));
    }

    // free GPU memory
    if (mean_) {
        check_cuda(cudaFree(d_sum));
    }
    check_cuda(cudaFree(d_incoming_gradients));
    check_cuda(cudaFree(d_gradients));

    return &gradients_;
}

// PIPELINED --- PIPELINED --- PIPELINED

GraphConvPipelined::GraphConvPipelined() {}

GraphConvPipelined::GraphConvPipelined(CudaHelper *helper, SparseMatrix<float> *adjacency, std::string reduction, long num_features,
                                       long chunk_size, long num_nodes) {
    set(helper, adjacency, reduction, num_features, chunk_size, num_nodes);
}

void GraphConvPipelined::set(CudaHelper *helper, SparseMatrix<float> *adjacency, std::string reduction, long num_features,
                             long chunk_size, long num_nodes) {
    GraphConvChunked::set(helper, adjacency, reduction, num_features, chunk_size, num_nodes);

    name_ = "feature-aggregation_pipelined";
    num_steps_ = 2;
    d_x_ = std::vector<float *>(num_steps_);
    d_adj_ = std::vector<SparseMatrixCuda<float>>(num_steps_);
    d_incoming_gradients_ = std::vector<float *>(num_steps_);
    d_sum_backward_ = std::vector<float *>(num_steps_);
}

std::vector<Matrix<float>> *GraphConvPipelined::forward(std::vector<Matrix<float>> *x) {
    for (int i = 0; i < num_chunks_; ++i) {
        to_column_major_inplace(&x->at(i));
    }

    check_cuda(cudaMalloc(&d_y_, y_.at(0).size_ * sizeof(float)));
    if (mean_) {
        check_cuda(cudaMalloc(&d_sum_forward_, y_.at(0).num_rows_ * sizeof(float)));
    }
    long adj_max_nnz = max_nnz(&adjacencies_);
    for (int i = 0; i < num_steps_; ++i) {
        d_adj_.at(i).set(chunk_size_, chunk_size_, adj_max_nnz);
        check_cuda(cudaMalloc(&d_x_.at(i), x->at(0).size_ * sizeof(float)));
    }

    for (long row = 0; row < num_chunks_; ++row) {
        check_cuda(cudaMemsetAsync(d_y_, 0, y_.at(row).size_ * sizeof(float),
                                   cuda_helper_->stream_in_));

        if (mean_) {
            check_cuda(cudaMemcpyAsync(d_sum_forward_, &sum_.values_[row * chunk_size_],
                                       y_.at(row).num_rows_ * sizeof(float), cudaMemcpyHostToDevice, cuda_helper_->stream_in_));
        }

        for (long column = 0; column < num_chunks_ + 1; ++column) {
            long column_a = (column / 2) * 2;
            long column_b = ((column - 1) / 2) * 2 + 1;
            SparseMatrix<float> *adj_a;
            SparseMatrix<float> *adj_b;
            if (column_a < num_chunks_)
                adj_a = &adjacencies_.at(row * num_chunks_ + column_a);
            if (column_b < num_chunks_)
                adj_b = &adjacencies_.at(row * num_chunks_ + column_b);

            if (column % 2 == 0) {
                // a in, b compute
                if (column_a < num_chunks_ && adj_a->nnz_ > 0) {
                    memcpy_sp_mat_async(&d_adj_.at(0), adj_a, cuda_helper_->stream_in_);

                    check_cuda(cudaMemcpyAsync(d_x_.at(0), x->at(column_a).values_, x->at(column_a).size_ * sizeof(float),
                                               cudaMemcpyHostToDevice, cuda_helper_->stream_in_));
                }

                if (column > 0 && adj_b->nnz_ > 0) {
                    sp_mat_mat_multi_cuda(cuda_helper_, &d_adj_.at(1), d_x_.at(1), d_y_,
                                          x->at(column_b).num_columns_, true);
                }
            } else {
                // b in, a compute
                if (column_b < num_chunks_ && adj_b->nnz_ > 0) {
                    memcpy_sp_mat_async(&d_adj_.at(1), adj_b, cuda_helper_->stream_in_);

                    check_cuda(cudaMemcpyAsync(d_x_.at(1), x->at(column_b).values_, x->at(column_b).size_ * sizeof(float),
                                               cudaMemcpyHostToDevice, cuda_helper_->stream_in_));
                }

                if (adj_a->nnz_ > 0) {
                    sp_mat_mat_multi_cuda(cuda_helper_, &d_adj_.at(0), d_x_.at(0), d_y_,
                                          x->at(column_a).num_columns_, true);
                }
            }

            check_cuda(cudaDeviceSynchronize());
        }

        if (mean_) {
            div_mat_vec(d_y_, d_sum_forward_, y_.at(row).num_rows_, y_.at(row).num_columns_);
        }

        check_cuda(cudaMemcpyAsync(y_.at(row).values_, d_y_, y_.at(row).size_ * sizeof(float),
                                   cudaMemcpyDeviceToHost, cuda_helper_->stream_out_));
    }

    check_cuda(cudaDeviceSynchronize());

    // free
    check_cuda(cudaFree(d_y_));
    if (mean_) {
        check_cuda(cudaFree(d_sum_forward_));
    }
    for (long i = 0; i < num_steps_; ++i) {
        check_cuda(cudaFree(d_x_.at(i)));
        d_adj_.at(i).free();
    }

    return &y_;
}

std::vector<Matrix<float>> *GraphConvPipelined::backward(std::vector<Matrix<float>> *incoming_gradients) {
    for (long i = 0; i < num_chunks_; ++i) {
        to_column_major_inplace(&incoming_gradients->at(i));
    }

    check_cuda(cudaMalloc(&d_gradients_, gradients_.at(0).size_ * sizeof(float)));

    long adj_max_nnz = max_nnz(&adjacencies_);
    for (long i = 0; i < num_steps_; ++i) {
        d_adj_.at(i).set(chunk_size_, chunk_size_, adj_max_nnz);
        check_cuda(cudaMalloc(&d_incoming_gradients_.at(i), incoming_gradients->at(0).size_ * sizeof(float)));
        if (mean_) {
            check_cuda(cudaMalloc(&d_sum_backward_.at(i), incoming_gradients->at(0).num_rows_ * sizeof(float)));
        }
    }

    for (long row = 0; row < num_chunks_; ++row) {
        check_cuda(cudaMemsetAsync(d_gradients_, 0, gradients_.at(row).size_ * sizeof(float), cuda_helper_->stream_in_));

        for (long column = 0; column < num_chunks_ + 1; ++column) {
            long column_a = (column / 2) * 2;
            long column_b = ((column - 1) / 2) * 2 + 1;
            SparseMatrix<float> *adj_a;
            SparseMatrix<float> *adj_b;
            if (column_a < num_chunks_)
                adj_a = &adjacencies_.at(row * num_chunks_ + column_a);
            if (column_b < num_chunks_)
                adj_b = &adjacencies_.at(row * num_chunks_ + column_b);

            if (column % 2 == 0) {
                // a in, b compute
                if (column_a < num_chunks_ && adj_a->nnz_ > 0) {
                    memcpy_sp_mat_async(&d_adj_.at(0), adj_a, cuda_helper_->stream_in_);

                    check_cuda(cudaMemcpyAsync(d_incoming_gradients_.at(0), incoming_gradients->at(column_a).values_,
                                               incoming_gradients->at(column_a).size_ * sizeof(float), cudaMemcpyHostToDevice,
                                               cuda_helper_->stream_in_));

                    if (mean_) {
                        check_cuda(cudaMemcpyAsync(d_sum_backward_.at(0), &sum_.values_[column_a * chunk_size_],
                                                   incoming_gradients->at(column_a).num_rows_ * sizeof(float),
                                                   cudaMemcpyHostToDevice, cuda_helper_->stream_in_));
                    }
                }

                if (column > 0 && adj_b->nnz_ > 0) {
                    if (mean_) {
                        div_mat_vec(d_incoming_gradients_.at(1), d_sum_backward_.at(1), incoming_gradients->at(column_b).num_rows_,
                                    incoming_gradients->at(column_b).num_columns_);
                    }

                    sp_mat_mat_multi_cuda(cuda_helper_, &d_adj_.at(1), d_incoming_gradients_.at(1), d_gradients_,
                                          incoming_gradients->at(column_b).num_columns_, true);
                }
            } else {
                // b in, a compute
                if (column_b < num_chunks_ && adj_b->nnz_ > 0) {
                    memcpy_sp_mat_async(&d_adj_.at(1), adj_b, cuda_helper_->stream_in_);

                    check_cuda(cudaMemcpyAsync(d_incoming_gradients_.at(1), incoming_gradients->at(column_b).values_,
                                               incoming_gradients->at(column_b).size_ * sizeof(float), cudaMemcpyHostToDevice,
                                               cuda_helper_->stream_in_));

                    if (mean_) {
                        check_cuda(cudaMemcpyAsync(d_sum_backward_.at(1), &sum_.values_[column_b * chunk_size_],
                                                   incoming_gradients->at(column_b).num_rows_ * sizeof(float),
                                                   cudaMemcpyHostToDevice, cuda_helper_->stream_in_));
                    }
                }

                if (column > 0 && adj_a->nnz_ > 0) {
                    if (mean_) {
                        div_mat_vec(d_incoming_gradients_.at(0), d_sum_backward_.at(0), incoming_gradients->at(column_a).num_rows_,
                                    incoming_gradients->at(column_a).num_columns_);
                    }

                    sp_mat_mat_multi_cuda(cuda_helper_, &d_adj_.at(0), d_incoming_gradients_.at(0), d_gradients_,
                                          incoming_gradients->at(column_a).num_columns_, true);
                }
            }

            check_cuda(cudaDeviceSynchronize());
        }

        check_cuda(cudaMemcpy(gradients_.at(row).values_, d_gradients_, gradients_.at(row).size_ * sizeof(float),
                              cudaMemcpyDeviceToHost));
    }

    check_cuda(cudaDeviceSynchronize());

    // free GPU memory
    check_cuda(cudaFree(d_gradients_));
    for (long i = 0; i < num_steps_; ++i) {
        d_adj_.at(i).free();
        check_cuda(cudaFree(d_incoming_gradients_.at(i)));
        if (mean_) {
            check_cuda(cudaFree(d_sum_backward_.at(i)));
        }
    }

    return &gradients_;
}
