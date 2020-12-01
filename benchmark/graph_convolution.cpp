// Copyright 2020 Marcel Wagenländer

#include "graph_convolution.hpp"
#include "cuda_helper.hpp"
#include "gpu_memory.hpp"
#include "tensors.hpp"
#include "gpu_memory_logger.hpp"

#include <benchmark/benchmark.h>


const std::string home = std::getenv("HOME");
const std::string dir_path = home + "/gpu_memory_reduction/alzheimer/data";
const std::string flickr_dir_path = dir_path + "/flickr";
const std::string reddit_dir_path = dir_path + "/reddit";
const std::string products_dir_path = dir_path + "/products";


static void BM_Graph_Convolution_Flickr_Forward(benchmark::State &state) {
    std::string path;
    path = flickr_dir_path + "/features.npy";
    Matrix<float> features = load_npy_matrix<float>(path);
    path = flickr_dir_path + "/adjacency.mtx";
    SparseMatrix<float> adjacency = load_mtx_matrix<float>(path);

    CudaHelper cuda_helper;
    GraphConvolution graph_convolution(&cuda_helper, &adjacency, "mean", features.num_rows_, features.num_columns_);

    GPUMemoryLogger memory_logger("flickr_forward");
    memory_logger.start();

    Matrix<float> *activations;
    for (auto _ : state) {
        activations = graph_convolution.forward(&features);
    }

    memory_logger.stop();
}
BENCHMARK(BM_Graph_Convolution_Flickr_Forward);

static void BM_Graph_Convolution_Chunked_Flickr_Forward(benchmark::State &state) {
    std::string path;
    path = flickr_dir_path + "/features.npy";
    Matrix<float> features = load_npy_matrix<float>(path);
    path = flickr_dir_path + "/adjacency.mtx";
    SparseMatrix<float> adjacency = load_mtx_matrix<float>(path);

    CudaHelper cuda_helper;
    GraphConvChunked graph_convolution(&cuda_helper, &adjacency, "mean", features.num_columns_, state.range(0), features.num_rows_);

    GPUMemoryLogger memory_logger("flickr_forward_" + std::to_string(state.range(0)));
    memory_logger.start();

    Matrix<float> *activations;
    for (auto _ : state) {
        activations = graph_convolution.forward(&features);
    }

    memory_logger.stop();
}
BENCHMARK(BM_Graph_Convolution_Chunked_Flickr_Forward)->Range(1 << 10, 1 << 15);

static void BM_Graph_Convolution_Flickr_Backward(benchmark::State &state) {
    std::string path;
    path = flickr_dir_path + "/features.npy";
    Matrix<float> features = load_npy_matrix<float>(path);
    path = flickr_dir_path + "/adjacency.mtx";
    SparseMatrix<float> adjacency = load_mtx_matrix<float>(path);
    Matrix<float> in_gradients(features.num_rows_, features.num_columns_, true);
    in_gradients.set_values(true);

    CudaHelper cuda_helper;
    GraphConvolution graph_convolution(&cuda_helper, &adjacency, "mean", features.num_rows_, features.num_columns_);

    Matrix<float> *activations = graph_convolution.forward(&features);

    GPUMemoryLogger memory_logger("flickr_backward");
    memory_logger.start();

    Matrix<float> *gradients;
    for (auto _ : state) {
        gradients = graph_convolution.backward(&in_gradients);
    }

    memory_logger.stop();
}
BENCHMARK(BM_Graph_Convolution_Flickr_Backward);

static void BM_Graph_Convolution_Flickr_Chunked_Backward(benchmark::State &state) {
    std::string path;
    path = flickr_dir_path + "/features.npy";
    Matrix<float> features = load_npy_matrix<float>(path);
    path = flickr_dir_path + "/adjacency.mtx";
    SparseMatrix<float> adjacency = load_mtx_matrix<float>(path);
    Matrix<float> in_gradients(features.num_rows_, features.num_columns_, true);
    in_gradients.set_values(true);

    CudaHelper cuda_helper;
    GraphConvChunked graph_convolution(&cuda_helper, &adjacency, "mean", features.num_columns_, state.range(0), features.num_rows_);

    Matrix<float> *activations = graph_convolution.forward(&features);

    GPUMemoryLogger memory_logger("flickr_backward_" + std::to_string(state.range(0)));
    memory_logger.start();

    Matrix<float> *gradients;
    for (auto _ : state) {
        gradients = graph_convolution.backward(&in_gradients);
    }

    memory_logger.stop();
}
BENCHMARK(BM_Graph_Convolution_Flickr_Chunked_Backward)->Range(1 << 10, 1 << 15);

static void BM_Graph_Convolution_Reddit_Forward(benchmark::State &state) {
    std::string path;
    path = reddit_dir_path + "/features.npy";
    Matrix<float> features = load_npy_matrix<float>(path);
    path = reddit_dir_path + "/adjacency.mtx";
    SparseMatrix<float> adjacency = load_mtx_matrix<float>(path);

    CudaHelper cuda_helper;
    GraphConvolution graph_convolution(&cuda_helper, &adjacency, "mean", features.num_rows_, features.num_columns_);

    GPUMemoryLogger memory_logger("reddit_forward");
    memory_logger.start();

    Matrix<float> *activations;
    for (auto _ : state) {
        activations = graph_convolution.forward(&features);
    }

    memory_logger.stop();
}
BENCHMARK(BM_Graph_Convolution_Reddit_Forward);

static void BM_Graph_Convolution_Reddit_Chunked_Forward(benchmark::State &state) {
    std::string path;
    path = reddit_dir_path + "/features.npy";
    Matrix<float> features = load_npy_matrix<float>(path);
    path = reddit_dir_path + "/adjacency.mtx";
    SparseMatrix<float> adjacency = load_mtx_matrix<float>(path);

    CudaHelper cuda_helper;
    GraphConvChunked graph_convolution(&cuda_helper, &adjacency, "mean", features.num_columns_, state.range(0), features.num_rows_);

    GPUMemoryLogger memory_logger("reddit_forward_" + std::to_string(state.range(0)));
    memory_logger.start();

    Matrix<float> *activations;
    for (auto _ : state) {
        activations = graph_convolution.forward(&features);
    }

    memory_logger.stop();
}
BENCHMARK(BM_Graph_Convolution_Reddit_Chunked_Forward)->Range(1 << 12, 1 << 17);

static void BM_Graph_Reddit_Convolution_Backward(benchmark::State &state) {
    std::string path;
    path = reddit_dir_path + "/features.npy";
    Matrix<float> features = load_npy_matrix<float>(path);
    path = reddit_dir_path + "/adjacency.mtx";
    SparseMatrix<float> adjacency = load_mtx_matrix<float>(path);
    Matrix<float> in_gradients(features.num_rows_, features.num_columns_, true);
    in_gradients.set_values(true);

    CudaHelper cuda_helper;
    GraphConvolution graph_convolution(&cuda_helper, &adjacency, "mean", features.num_rows_, features.num_columns_);

    Matrix<float> *activations = graph_convolution.forward(&features);

    GPUMemoryLogger memory_logger("reddit_backward");
    memory_logger.start();

    Matrix<float> *gradients;
    for (auto _ : state) {
        gradients = graph_convolution.backward(&in_gradients);
    }

    memory_logger.stop();
}
BENCHMARK(BM_Graph_Reddit_Convolution_Backward);

static void BM_Graph_Convolution_Reddit_Chunked_Backward(benchmark::State &state) {
    std::string path;
    path = reddit_dir_path + "/features.npy";
    Matrix<float> features = load_npy_matrix<float>(path);
    path = reddit_dir_path + "/adjacency.mtx";
    SparseMatrix<float> adjacency = load_mtx_matrix<float>(path);
    Matrix<float> in_gradients(features.num_rows_, features.num_columns_, true);
    in_gradients.set_values(true);

    CudaHelper cuda_helper;
    GraphConvChunked graph_convolution(&cuda_helper, &adjacency, "mean", features.num_columns_, state.range(0), features.num_rows_);

    Matrix<float> *activations = graph_convolution.forward(&features);

    GPUMemoryLogger memory_logger("reddit_backward_" + std::to_string(state.range(0)));
    memory_logger.start();

    Matrix<float> *gradients;
    for (auto _ : state) {
        gradients = graph_convolution.backward(&in_gradients);
    }

    memory_logger.stop();
}
BENCHMARK(BM_Graph_Convolution_Reddit_Chunked_Backward)->Range(1 << 12, 1 << 17);

static void BM_Graph_Convolution_Products_Forward(benchmark::State &state) {
    std::string path;
    path = products_dir_path + "/features.npy";
    Matrix<float> features = load_npy_matrix<float>(path);
    path = products_dir_path + "/adjacency.mtx";
    SparseMatrix<float> adjacency = load_mtx_matrix<float>(path);

    CudaHelper cuda_helper;
    GraphConvolution graph_convolution(&cuda_helper, &adjacency, "mean", features.num_rows_, features.num_columns_);

    GPUMemoryLogger memory_logger("products_forward");
    memory_logger.start();

    Matrix<float> *activations;
    for (auto _ : state) {
        activations = graph_convolution.forward(&features);
    }

    memory_logger.stop();
}
BENCHMARK(BM_Graph_Convolution_Products_Forward);

static void BM_Graph_Convolution_Products_Chunked_Forward(benchmark::State &state) {
    std::string path;
    path = products_dir_path + "/features.npy";
    Matrix<float> features = load_npy_matrix<float>(path);
    path = products_dir_path + "/adjacency.mtx";
    SparseMatrix<float> adjacency = load_mtx_matrix<float>(path);

    CudaHelper cuda_helper;
    GraphConvChunked graph_convolution(&cuda_helper, &adjacency, "mean", features.num_columns_, state.range(0), features.num_rows_);

    GPUMemoryLogger memory_logger("products_forward_" + std::to_string(state.range(0)));
    memory_logger.start();

    Matrix<float> *activations;
    for (auto _ : state) {
        activations = graph_convolution.forward(&features);
    }

    memory_logger.stop();
}
BENCHMARK(BM_Graph_Convolution_Products_Chunked_Forward)->Range(1 << 16, 1 << 21);

static void BM_Graph_Products_Convolution_Backward(benchmark::State &state) {
    std::string path;
    path = products_dir_path + "/features.npy";
    Matrix<float> features = load_npy_matrix<float>(path);
    path = products_dir_path + "/adjacency.mtx";
    SparseMatrix<float> adjacency = load_mtx_matrix<float>(path);
    Matrix<float> in_gradients(features.num_rows_, features.num_columns_, true);
    in_gradients.set_values(true);

    CudaHelper cuda_helper;
    GraphConvolution graph_convolution(&cuda_helper, &adjacency, "mean", features.num_rows_, features.num_columns_);

    Matrix<float> *activations = graph_convolution.forward(&features);

    GPUMemoryLogger memory_logger("products_backward");
    memory_logger.start();

    Matrix<float> *gradients;
    for (auto _ : state) {
        gradients = graph_convolution.backward(&in_gradients);
    }

    memory_logger.stop();
}
BENCHMARK(BM_Graph_Products_Convolution_Backward);

static void BM_Graph_Convolution_Products_Chunked_Backward(benchmark::State &state) {
    std::string path;
    path = products_dir_path + "/features.npy";
    Matrix<float> features = load_npy_matrix<float>(path);
    path = products_dir_path + "/adjacency.mtx";
    SparseMatrix<float> adjacency = load_mtx_matrix<float>(path);
    Matrix<float> in_gradients(features.num_rows_, features.num_columns_, true);
    in_gradients.set_values(true);

    CudaHelper cuda_helper;
    GraphConvChunked graph_convolution(&cuda_helper, &adjacency, "mean", features.num_columns_, state.range(0), features.num_rows_);

    Matrix<float> *activations = graph_convolution.forward(&features);

    GPUMemoryLogger memory_logger("products_backward_" + std::to_string(state.range(0)));
    memory_logger.start();

    Matrix<float> *gradients;
    for (auto _ : state) {
        gradients = graph_convolution.backward(&in_gradients);
    }

    memory_logger.stop();
}
BENCHMARK(BM_Graph_Convolution_Products_Chunked_Backward)->Range(1 << 16, 1 << 21);
