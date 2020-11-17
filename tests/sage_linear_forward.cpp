// Copyright 2020 Marcel Wagenländer

#include "cuda_helper.hpp"
#include "helper.hpp"
#include "sage_linear.hpp"
#include "tensors.hpp"

#include "catch2/catch.hpp"
#include <string>

const std::string home = std::getenv("HOME");
const std::string dir_path = home + "/gpu_memory_reduction/alzheimer/data";
const std::string flickr_dir_path = dir_path + "/flickr";
const std::string test_dir_path = dir_path + "/tests";


int test_sage_linear_forward(matrix<float> input_self, matrix<float> input_neigh) {
    std::string path;

    CudaHelper cuda_helper;
    int num_out_features = 256;
    SageLinear sage_linear(input_self.columns, num_out_features, &cuda_helper);

    matrix<float> result = sage_linear.forward(input_self, input_neigh);

    path = test_dir_path + "/result.npy";
    save_npy_matrix(result, path);

    save_params(sage_linear.get_parameters());

    char command[] = "/home/ubuntu/gpu_memory_reduction/pytorch-venv/bin/python3 /home/ubuntu/gpu_memory_reduction/alzheimer/tests/sage_linear_forward.py";
    system(command);

    path = test_dir_path + "/value.npy";
    matrix<int> value = load_npy_matrix<int>(path);

    return value.values[0];
}

int test_sage_linear_forward_chunked(matrix<float> input_self, matrix<float> input_neigh, int chunk_size) {
    std::string path;

    CudaHelper cuda_helper;
    int num_nodes = input_self.rows;
    int num_out_features = 101;
    SageLinearChunked sage_linear_chunked(&cuda_helper, input_self.columns, num_out_features, chunk_size, num_nodes);

    matrix<float> result = sage_linear_chunked.forward(input_self, input_neigh);

    path = test_dir_path + "/result.npy";
    save_npy_matrix(result, path);

    save_params(sage_linear_chunked.get_parameters());

    char command[] = "/home/ubuntu/gpu_memory_reduction/pytorch-venv/bin/python3 /home/ubuntu/gpu_memory_reduction/alzheimer/tests/sage_linear_forward.py";
    system(command);

    path = test_dir_path + "/value.npy";
    matrix<int> value = load_npy_matrix<int>(path);

    return value.values[0];
}


TEST_CASE("SageLinear forward", "[sagelinear][forward]") {
    std::string path;
    int rows = 1 << 15;
    int columns = 1 << 9;

    matrix<float> input_self = gen_rand_matrix(rows, columns);
    path = test_dir_path + "/input_self.npy";
    save_npy_matrix(input_self, path);

    matrix<float> input_neigh = gen_rand_matrix(rows, columns);
    path = test_dir_path + "/input_neigh.npy";
    save_npy_matrix(input_neigh, path);

    CHECK(test_sage_linear_forward(input_self, input_neigh));
}

TEST_CASE("SageLinear forward, non-random input", "[sagelinear][forward][nonrandom]") {
    std::string path;
    int rows = 1 << 15;
    int columns = 1 << 9;

    matrix<float> input_self = gen_non_rand_matrix(rows, columns);
    path = test_dir_path + "/input_self.npy";
    save_npy_matrix(input_self, path);

    matrix<float> input_neigh = gen_non_rand_matrix(rows, columns);
    path = test_dir_path + "/input_neigh.npy";
    save_npy_matrix(input_neigh, path);

    CHECK(test_sage_linear_forward(input_self, input_neigh));
}

TEST_CASE("SageLinear forward, chunked", "[sagelinear][forward][chunked]") {
    std::string path;
    int rows = 1 << 15;
    int columns = 1 << 9;

    matrix<float> input_self = gen_rand_matrix(rows, columns);
    path = test_dir_path + "/input_self.npy";
    save_npy_matrix(input_self, path);

    matrix<float> input_neigh = gen_rand_matrix(rows, columns);
    path = test_dir_path + "/input_neigh.npy";
    save_npy_matrix(input_neigh, path);

    CHECK(test_sage_linear_forward_chunked(input_self, input_neigh, 1 << 15));
    CHECK(test_sage_linear_forward_chunked(input_self, input_neigh, 1 << 12));
    CHECK(test_sage_linear_forward_chunked(input_self, input_neigh, 1 << 8));
}


TEST_CASE("SageLinear forward, chunked, small", "[sagelinear][forward][chunked][small]") {
    std::string path;
    int rows = 1 << 5;
    int columns = 1 << 4;

    matrix<float> input_self = gen_rand_matrix(rows, columns);
    path = test_dir_path + "/input_self.npy";
    save_npy_matrix(input_self, path);

    matrix<float> input_neigh = gen_rand_matrix(rows, columns);
    path = test_dir_path + "/input_neigh.npy";
    save_npy_matrix(input_neigh, path);

    CHECK(test_sage_linear_forward_chunked(input_self, input_neigh, 1 << 5));
    CHECK(test_sage_linear_forward_chunked(input_self, input_neigh, 1 << 2));
    CHECK(test_sage_linear_forward_chunked(input_self, input_neigh, 1 << 1));
}

TEST_CASE("SageLinear forward, chunked, non-random input", "[sagelinear][forward][chunked][nonrandom]") {
    std::string path;
    int rows = 1 << 15;
    int columns = 1 << 9;

    matrix<float> input_self = gen_non_rand_matrix(rows, columns);
    path = test_dir_path + "/input_self.npy";
    save_npy_matrix(input_self, path);

    matrix<float> input_neigh = gen_non_rand_matrix(rows, columns);
    path = test_dir_path + "/input_neigh.npy";
    save_npy_matrix(input_neigh, path);

    CHECK(test_sage_linear_forward_chunked(input_self, input_neigh, 1 << 15));
    CHECK(test_sage_linear_forward_chunked(input_self, input_neigh, 1 << 12));
    CHECK(test_sage_linear_forward_chunked(input_self, input_neigh, 1 << 8));
}

TEST_CASE("SageLinear forward, chunked,non-random input, small", "[sagelinear][forward][chunked][nonrandom][small]") {
    std::string path;
    int rows = 1 << 5;
    int columns = 1 << 4;

    matrix<float> input_self = gen_non_rand_matrix(rows, columns);
    path = test_dir_path + "/input_self.npy";
    save_npy_matrix(input_self, path);

    matrix<float> input_neigh = gen_non_rand_matrix(rows, columns);
    path = test_dir_path + "/input_neigh.npy";
    save_npy_matrix(input_neigh, path);

    CHECK(test_sage_linear_forward_chunked(input_self, input_neigh, 1 << 5));
    CHECK(test_sage_linear_forward_chunked(input_self, input_neigh, 1 << 2));
    CHECK(test_sage_linear_forward_chunked(input_self, input_neigh, 1 << 1));
}
