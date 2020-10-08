// Copyright 2020 Marcel Wagenländer

#include "graph_convolution.hpp"

#include "cuda_helper.hpp"


matrix<float> graph_convolution(sparse_matrix<float> A, matrix<float> B,
        std::string reduction) {
    bool mean;
    if (reduction.compare("mean") == 0) {
        mean = true;
    } else if (reduction.compare("sum") == 0) {
        mean = false;
    } else {
        std::cout << "Reduction not supported" << std::endl;
    }

    cudaError_t cuda_error = cudaSuccess;
    cusparseStatus_t sparse_status = CUSPARSE_STATUS_SUCCESS;
    cusparseHandle_t sparse_handle;
    sparse_status = cusparseCreate(&sparse_handle);
    check_cusparse(sparse_status);


    float *d_A_csr_val;
    int *d_A_csr_row_offsets, *d_A_col_ind;
    cuda_error = cudaMalloc((void**) &d_A_csr_val, 
            A.nnz * sizeof(float));
    check_cuda(cuda_error);
    cuda_error = cudaMalloc((void**) &d_A_csr_row_offsets,
            (A.rows + 1) * sizeof(int));
    check_cuda(cuda_error);
    cuda_error = cudaMalloc((void**) &d_A_col_ind,
            A.nnz * sizeof(int));
    check_cuda(cuda_error);
    cuda_error = cudaMemcpy(d_A_csr_val, A.csr_val,
            A.nnz * sizeof(float), cudaMemcpyHostToDevice);
    check_cuda(cuda_error);
    cuda_error = cudaMemcpy(d_A_csr_row_offsets, A.csr_row_ptr,
            (A.rows + 1) * sizeof(int), cudaMemcpyHostToDevice);
    check_cuda(cuda_error);
   cuda_error = cudaMemcpy(d_A_col_ind, A.csr_col_ind,
            A.nnz * sizeof(int), cudaMemcpyHostToDevice);
    check_cuda(cuda_error);
    cusparseSpMatDescr_t A_descr;
    sparse_status = cusparseCreateCsr(&A_descr, A.rows,
            A.columns, A.nnz,
            d_A_csr_row_offsets, d_A_col_ind,
            d_A_csr_val,
            CUSPARSE_INDEX_32I, CUSPARSE_INDEX_32I,
            CUSPARSE_INDEX_BASE_ZERO, CUDA_R_32F);
    check_cusparse(sparse_status);
  
    // B to column-major
    matrix<float> B_col;
    B_col.rows = B.columns;
    B_col.columns = B.rows;
    B_col.values = (float*) malloc(B_col.rows * B_col.columns * sizeof(float));
    transpose<float>(B_col.values, B.values, B.rows, B.columns);
    
    //create cusparse B
    float *d_B;
    cuda_error = cudaMalloc((void**) &d_B, B.rows * B.columns * sizeof(float));
    check_cuda(cuda_error);
    cuda_error = cudaMemcpy(d_B, B_col.values, B.rows * B.columns * sizeof(float),
            cudaMemcpyHostToDevice);
    check_cuda(cuda_error);
    cusparseDnMatDescr_t B_descr;
    sparse_status = cusparseCreateDnMat(&B_descr, B.rows, B.columns,
            B.rows, d_B,
            CUDA_R_32F, CUSPARSE_ORDER_COL);
    check_cusparse(sparse_status);

    // create result
    matrix<float> result;
    result.rows = A.rows;
    result.columns = B.columns;
    result.values = (float*) malloc(result.rows * result.columns * sizeof(float));
    for (int i = 0; i < result.rows * result.columns; ++i) {
        result.values[i] = 0.0f;
    }
    // result to column-major
    matrix<float> result_col;
    result_col.rows = result.columns;
    result_col.columns = result.rows;
    result_col.values = (float*) malloc(result_col.rows * result_col.columns * sizeof(float));
    transpose<float>(result_col.values, result.values, result.rows, result.columns);

    // create cusparse result
    float *d_result;
    cuda_error = cudaMalloc((void**) &d_result, result.rows * result.columns * sizeof(float));
    check_cuda(cuda_error);
    cuda_error = cudaMemcpy(d_result, result_col.values, result_col.rows * result_col.columns * sizeof(float),
            cudaMemcpyHostToDevice);
    check_cuda(cuda_error);
    cusparseDnMatDescr_t result_descr;
    sparse_status = cusparseCreateDnMat(&result_descr, result.rows, result.columns,
            result.rows, d_result,
            CUDA_R_32F, CUSPARSE_ORDER_COL);
    check_cusparse(sparse_status);

    // get buffer size for SpMM
    float alpha = 1.0f;
    float beta = 0.0f;
    size_t buffer_size;
    sparse_status = cusparseSpMM_bufferSize(sparse_handle,
            CUSPARSE_OPERATION_NON_TRANSPOSE, CUSPARSE_OPERATION_NON_TRANSPOSE,
            &alpha, A_descr, B_descr, &beta, result_descr,
            // CUSPARSE_MM_ALG_DEFAULT is deprecated
            // but CUSPARSE_SPMM_ALG_DEFAULT is not working
            CUDA_R_32F, CUSPARSE_MM_ALG_DEFAULT,
            &buffer_size);
    check_cusparse(sparse_status);
    void *d_buffer;
    cuda_error = cudaMalloc(&d_buffer, buffer_size);
    check_cuda(cuda_error);

    // compute SpMM
    sparse_status = cusparseSpMM(sparse_handle,
            CUSPARSE_OPERATION_NON_TRANSPOSE, CUSPARSE_OPERATION_NON_TRANSPOSE,
            &alpha, A_descr, B_descr, &beta, result_descr,
            CUDA_R_32F, CUSPARSE_MM_ALG_DEFAULT,
            d_buffer);
    check_cusparse(sparse_status);

    cuda_error = cudaFree(d_buffer);
    check_cuda(cuda_error);

    // move result_col to CPU memory
    cuda_error = cudaMemcpy(result_col.values, d_result,
            result_col.rows * result_col.columns * sizeof(float),
            cudaMemcpyDeviceToHost);
    check_cuda(cuda_error);

    // result to row-major
    transpose<float>(result.values, result_col.values, result_col.rows, result_col.columns);

    // apply mean
    if (mean) {
        vector<float> ones;
        ones.size = A.rows;
        ones.values = (float *) malloc(ones.size * sizeof(float));
        for (int i = 0; i < ones.size; ++i) {
            ones.values[i] = 1.0;
        }
        float *d_ones;
        cuda_error = cudaMalloc(&d_ones, ones.size * sizeof(float));
        check_cuda(cuda_error);
        cuda_error = cudaMemcpy(d_ones, ones.values, ones.size * sizeof(float),
                cudaMemcpyHostToDevice);
        check_cuda(cuda_error);
        cusparseDnVecDescr_t ones_desc;
        sparse_status = cusparseCreateDnVec(&ones_desc, ones.size,
                d_ones, CUDA_R_32F);
        check_cusparse(sparse_status);

        vector<float> sum;
        sum.size = ones.size;
        sum.values = (float *) malloc(sum.size * sizeof(float));
        for (int i = 0; i < sum.size; ++i) {
            sum.values[0] = 0.0;
        }
        float *d_sum;
        cuda_error = cudaMalloc(&d_sum, sum.size * sizeof(float));
        check_cuda(cuda_error);
        cuda_error = cudaMemcpy(d_sum, sum.values, sum.size * sizeof(float),
                cudaMemcpyHostToDevice);
        cusparseDnVecDescr_t sum_desc;
        sparse_status = cusparseCreateDnVec(&sum_desc, sum.size,
                d_sum, CUDA_R_32F);
        check_cusparse(sparse_status);

        sparse_status = cusparseSpMV_bufferSize(sparse_handle,
                CUSPARSE_OPERATION_NON_TRANSPOSE,
                &alpha, A_descr, ones_desc,
                &beta, sum_desc,
                CUDA_R_32F, CUSPARSE_MV_ALG_DEFAULT, &buffer_size);
        check_cusparse(sparse_status);
        cuda_error = cudaMalloc(&d_buffer, buffer_size);
        check_cuda(cuda_error);
        sparse_status = cusparseSpMV(sparse_handle,
                CUSPARSE_OPERATION_NON_TRANSPOSE,
                &alpha, A_descr, ones_desc,
                &beta, sum_desc,
                CUDA_R_32F, CUSPARSE_MV_ALG_DEFAULT, d_buffer);
        check_cusparse(sparse_status);

        cuda_error = cudaMemcpy(sum.values, d_sum,
                sum.size * sizeof(float),
                cudaMemcpyDeviceToHost);
        check_cuda(cuda_error);

        // TODO do the following on GPU
        // scale by 1 / sum
        for (int i = 0; i < result.rows; ++i) {
            for (int j = 0; j < result.columns; ++j) {
                result.values[i * result.columns + j] = result.values[i * result.columns + j] / sum.values[i];
            }
        }

        // free GPU memory
        cuda_error = cudaFree(d_ones);
        check_cuda(cuda_error);
        cuda_error = cudaFree(d_sum);
        check_cuda(cuda_error);

        // free CPU memory
        free(ones.values);
        free(sum.values);
    }  // end mean

    // free memory
    cuda_error = cudaFree(d_A_csr_val);
    check_cuda(cuda_error);
    cuda_error = cudaFree(d_A_col_ind);
    check_cuda(cuda_error);
    cuda_error = cudaFree(d_A_csr_row_offsets);
    check_cuda(cuda_error);
    cuda_error = cudaFree(d_B);
    check_cuda(cuda_error);
    cuda_error = cudaFree(d_buffer);
    check_cuda(cuda_error);
    cuda_error = cudaFree(d_result);
    check_cuda(cuda_error);

    sparse_status = cusparseDestroy(sparse_handle);
    check_cusparse(sparse_status);

    return result;
}





















matrix<float> graph_convolution_debug(sparse_matrix<float> A, matrix<float> B) {
    cudaError_t cuda_error = cudaSuccess;
    cusparseStatus_t sparse_status = CUSPARSE_STATUS_SUCCESS;
    cusparseHandle_t sparse_handle;
    sparse_status = cusparseCreate(&sparse_handle);
    check_cusparse(sparse_status);

    float *d_A_csr_val;
    int *d_A_csr_row_offsets, *d_A_col_ind;
    cuda_error = cudaMalloc((void**) &d_A_csr_val, 
            A.nnz * sizeof(float));
    check_cuda(cuda_error);
    cuda_error = cudaMalloc((void**) &d_A_csr_row_offsets,
            (A.rows + 1) * sizeof(int));
    check_cuda(cuda_error);
    cuda_error = cudaMalloc((void**) &d_A_col_ind,
            A.nnz * sizeof(int));
    check_cuda(cuda_error);
    cuda_error = cudaMemcpy(d_A_csr_val, A.csr_val,
            A.nnz * sizeof(float), cudaMemcpyHostToDevice);
    check_cuda(cuda_error);
    cuda_error = cudaMemcpy(d_A_csr_row_offsets, A.csr_row_ptr,
            (A.rows + 1) * sizeof(int), cudaMemcpyHostToDevice);
    check_cuda(cuda_error);
   cuda_error = cudaMemcpy(d_A_col_ind, A.csr_col_ind,
            A.nnz * sizeof(int), cudaMemcpyHostToDevice);
    check_cuda(cuda_error);
    cusparseSpMatDescr_t A_descr;
    sparse_status = cusparseCreateCsr(&A_descr, A.rows,
            A.columns, A.nnz,
            d_A_csr_row_offsets, d_A_col_ind,
            d_A_csr_val,
            CUSPARSE_INDEX_32I, CUSPARSE_INDEX_32I,
            CUSPARSE_INDEX_BASE_ZERO, CUDA_R_32F);
    check_cusparse(sparse_status);
  
    // B to column-major
    matrix<float> B_col;
    B_col.rows = B.columns;
    B_col.columns = B.rows;
    B_col.values = (float*) malloc(B_col.rows * B_col.columns * sizeof(float));
    transpose<float>(B_col.values, B.values, B.rows, B.columns);
    
    //create cusparse B
    float *d_B;
    cuda_error = cudaMalloc((void**) &d_B, B.rows * B.columns * sizeof(float));
    check_cuda(cuda_error);
    cuda_error = cudaMemcpy(d_B, B_col.values, B.rows * B.columns * sizeof(float),
            cudaMemcpyHostToDevice);
    check_cuda(cuda_error);
    cusparseDnMatDescr_t B_descr;
    sparse_status = cusparseCreateDnMat(&B_descr, B.rows, B.columns,
            B.rows, d_B,
            CUDA_R_32F, CUSPARSE_ORDER_COL);
    check_cusparse(sparse_status);

    // create result
    matrix<float> result;
    result.rows = A.rows;
    result.columns = B.columns;
    result.values = (float*) malloc(result.rows * result.columns * sizeof(float));
    for (int i = 0; i < result.rows * result.columns; ++i) {
        result.values[i] = 0.0f;
    }
    // result to column-major
    matrix<float> result_col;
    result_col.rows = result.columns;
    result_col.columns = result.rows;
    result_col.values = (float*) malloc(result_col.rows * result_col.columns * sizeof(float));
    transpose<float>(result_col.values, result.values, result.rows, result.columns);

    // create cusparse result
    float *d_result;
    cuda_error = cudaMalloc((void**) &d_result, result.rows * result.columns * sizeof(float));
    check_cuda(cuda_error);
    cuda_error = cudaMemcpy(d_result, result_col.values, result_col.rows * result_col.columns * sizeof(float),
            cudaMemcpyHostToDevice);
    check_cuda(cuda_error);
    cusparseDnMatDescr_t result_descr;
    sparse_status = cusparseCreateDnMat(&result_descr, result.rows, result.columns,
            result.rows, d_result,
            CUDA_R_32F, CUSPARSE_ORDER_COL);
    check_cusparse(sparse_status);

    // get buffer size for SpMM
    float alpha = 1.0f;
    float beta = 0.0f;
    size_t buffer_size;
    sparse_status = cusparseSpMM_bufferSize(sparse_handle,
            CUSPARSE_OPERATION_NON_TRANSPOSE, CUSPARSE_OPERATION_NON_TRANSPOSE,
            &alpha, A_descr, B_descr, &beta, result_descr,
            // CUSPARSE_MM_ALG_DEFAULT is deprecated
            // but CUSPARSE_SPMM_ALG_DEFAULT is not working
            CUDA_R_32F, CUSPARSE_MM_ALG_DEFAULT,
            &buffer_size);
    check_cusparse(sparse_status);
    void *d_buffer;
    cuda_error = cudaMalloc(&d_buffer, buffer_size);
    check_cuda(cuda_error);

    // compute SpMM
    sparse_status = cusparseSpMM(sparse_handle,
            CUSPARSE_OPERATION_NON_TRANSPOSE, CUSPARSE_OPERATION_NON_TRANSPOSE,
            &alpha, A_descr, B_descr, &beta, result_descr,
            CUDA_R_32F, CUSPARSE_MM_ALG_DEFAULT,
            d_buffer);
    check_cusparse(sparse_status);

    cuda_error = cudaFree(d_buffer);
    check_cuda(cuda_error);

    // move result_col to CPU memory
    cuda_error = cudaMemcpy(result_col.values, d_result,
            result_col.rows * result_col.columns * sizeof(float),
            cudaMemcpyDeviceToHost);
    check_cuda(cuda_error);

    // result to row-major
    transpose<float>(result.values, result_col.values, result_col.rows, result_col.columns);

    // free memory
    cuda_error = cudaFree(d_A_csr_val);
    check_cuda(cuda_error);
    cuda_error = cudaFree(d_A_col_ind);
    check_cuda(cuda_error);
    cuda_error = cudaFree(d_A_csr_row_offsets);
    check_cuda(cuda_error);
    cuda_error = cudaFree(d_B);
    check_cuda(cuda_error);
    cuda_error = cudaFree(d_buffer);
    check_cuda(cuda_error);
    cuda_error = cudaFree(d_result);
    check_cuda(cuda_error);

    sparse_status = cusparseDestroy(sparse_handle);
    check_cusparse(sparse_status);

    return result;
}
