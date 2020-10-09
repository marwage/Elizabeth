// Copyright 2020 Marcel Wagenländer

#include "tensors.hpp"

#include "cnpy.h"
#include "mmio_wrapper.hpp"

#include <iostream>
#include <cstring>


template <typename T>
void print_matrix(matrix<T> mat) {
    int N;
    if (mat.rows < 10) {
        N = mat.rows;
    } else {
        N = 10;
    }
    int M;
    if (mat.columns < 10) {
        M = mat.columns;
    } else {
        M = 10;
    }

    // for (int i = 0; i < rows; i = i + 1) {
    for (int i = 0; i < N; i = i + 1) {
        // for (int j = 0; j < cols; j = j + 1) {
        for (int j = 0; j < M; j = j + 1) {
            std::cout << mat.values[i * mat.columns + j] << ",";
        }
        std::cout << std::endl;
    }
}

template void print_matrix<float>(matrix<float> mat);
template void print_matrix<int>(matrix<int> mat);


int new_index(int old_idx, int N, int M) {
    int last_idx = M * N - 1;
    if (old_idx == last_idx) {
        return last_idx;
    } else {
        long int new_idx = (long int) N * (long int) old_idx;
        new_idx = new_idx % last_idx;
        return (int) new_idx;
    }
}

template <typename T>
void transpose(T *a_T, T *a, int rows, int cols) {
    int old_idx, new_idx;
    for (int i = 0; i < rows; i = i + 1) {
        for (int j = 0; j < cols; j = j + 1) {
            old_idx = i * cols + j;
            new_idx = new_index(old_idx, rows, cols);
            a_T[new_idx] = a[old_idx];
        }
    }
}

template <typename T>
void one_to_zero_index(T *a, int len) {
    for (int i = 0; i < len; ++i) {
        a[i] = a[i] - 1;
    }
}

template void one_to_zero_index<int>(int *a, int len);


template <typename T>
matrix<T> load_npy_matrix(std::string path) {
    cnpy::NpyArray arr = cnpy::npy_load(path);
    if (arr.word_size != sizeof(T)) {
        std::cout << "Matrix has wrong data type" << std::endl;
    }
    T *arr_data = arr.data<T>();
    matrix<T> mat;
    mat.rows = arr.shape[0];
    if (arr.shape.size() == 1) {
        mat.columns = 1;
    } else {
        mat.columns = arr.shape[1];
    }
    mat.values = reinterpret_cast<T *>(
            malloc(mat.rows * mat.columns * sizeof(T)));
    std::memcpy(mat.values, arr_data, mat.rows * mat.columns * sizeof(T));

    return mat;
}

template matrix<float> load_npy_matrix<float>(std::string path);
template matrix<int> load_npy_matrix<int>(std::string path);
template matrix<bool> load_npy_matrix<bool>(std::string path);


template <typename T>
sparse_matrix<T> load_mtx_matrix(std::string path) {
    char *path_char = &*path.begin();
    sparse_matrix<T> sp_mat;
    int err = loadMMSparseMatrix<T>(path_char, 'f', true,
            &sp_mat.rows, &sp_mat.columns, &sp_mat.nnz,
            &sp_mat.csr_val, &sp_mat.csr_row_ptr,
            &sp_mat.csr_col_ind, true);
    if (err) {
        std::cout << "loadMMSparseMatrix failed" << std::endl;
    }
    one_to_zero_index(sp_mat.csr_row_ptr, sp_mat.rows + 1);
    one_to_zero_index(sp_mat.csr_col_ind, sp_mat.nnz);

    return sp_mat;
}

template sparse_matrix<float> load_mtx_matrix<float>(std::string path);


template <typename T>
void save_npy_matrix(matrix<T> mat, std::string path) {
    std::vector<size_t> shape = {(size_t) mat.rows, (size_t) mat.columns};
    cnpy::npy_save<T>(path, mat.values, shape);
}

template void save_npy_matrix<float>(matrix<float> mat, std::string path);
template void save_npy_matrix<int>(matrix<int> mat, std::string path);


template <typename T>
void to_column_major(matrix<T> *mat) {
    T *new_values = reinterpret_cast<T *>(
            malloc(mat->rows * mat->columns * sizeof(T)));
    transpose<T>(new_values, mat->values, mat->rows, mat->columns);
    free(mat->values);
    mat->values = new_values;
}

template void to_column_major<float>(matrix<float> *mat);
template void to_column_major<int>(matrix<int> *mat);


template <typename T>
void to_row_major(matrix<T> *mat) {
    T *new_values = reinterpret_cast<T *>(
            malloc(mat->rows * mat->columns * sizeof(T)));
    transpose<T>(new_values, mat->values, mat->columns, mat->rows);
    free(mat->values);
    mat->values = new_values;
}

template void to_row_major<float>(matrix<float> *mat);
template void to_row_major<int>(matrix<int> *mat);
