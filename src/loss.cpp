// Copyright 2020 Marcel Wagenländer

#include "loss.hpp"

// debug
#include <iostream>


NLLLoss::NLLLoss(long num_nodes, long num_features) {
    gradients_ = new_float_matrix(num_nodes, num_features, false);
}

float NLLLoss::forward(matrix<float> X, matrix<int> y) {
    to_column_major_inplace(&X);

    float loss = 0.0;
    for (int i = 0; i < X.rows; ++i) {
        loss = loss + X.values[y.values[i] * X.rows + i];
    }
    loss = loss / (float) X.rows;
    loss = -loss;

    input_ = X;
    y_ = y;

    return static_cast<float>(loss);
}

matrix<float> NLLLoss::backward() {
    for (int i = 0; i < y_.rows; ++i) {
        gradients_.values[y_.values[i] * y_.rows + i] = -1.0 / input_.rows;
    }

    return gradients_;
}
