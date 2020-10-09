// Copyright 2020 Marcel Wagenländer

#ifndef TENSORS_H
#define TENSORS_H

#include <string>


template <typename T>
struct matrix {
    int rows;
    int columns;
    T *values;
};

template <typename T>
struct sparse_matrix {
    int rows;
    int columns;
    int nnz;
    T *csr_val;
    int *csr_row_ptr;
    int *csr_col_ind;
};

template <typename T>
void print_matrix(matrix<T> mat);

template <typename T>
matrix<T> load_npy_matrix(std::string path);

template <typename T>
sparse_matrix<T> load_mtx_matrix(std::string path);

template <typename T>
void save_npy_matrix(matrix<T> mat, std::string path);

template <typename T>
void to_column_major(matrix<T> *mat);

template <typename T>
void to_row_major(matrix<T> *mat);

#endif
