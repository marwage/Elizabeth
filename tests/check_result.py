import json
import numpy as np
import scipy.sparse as sp
import scipy.io
import os
import torch


home = os.getenv("HOME")
dir_path = home + "/gpu_memory_reduction/alzheimer/data/flickr"

# read matrices
path = dir_path + "/adjacency.mtx"
adj = scipy.io.mmread(path)

path = dir_path + "/features.npy"
features = np.load(path)

# check read/write
check_write = False
if check_write:
    path = dir_path + "/features_write.npy";
    features_write = np.load(path);

    assert(features_write.shape == features.shape)
    percentage_equal = (features_write == features).sum() / features.size

    print("Write")
    print("Percentage of equal elements: {}".format(percentage_equal))

# check dropout
# to degree it is possible
check_dropout = False
if check_dropout:
    path = dir_path + "/dropout_result.npy"
    dropout_result = np.load(path)

    probability = 0.2
    dropout_layer = torch.nn.Dropout(p=probability)
    features_torch = torch.from_numpy(features)
    torch_dropout_result = dropout_layer(features_torch)
    torch_dropout_result = torch_dropout_result.numpy()

    assert(dropout_result.shape == torch_dropout_result.shape)
    percentage_equal = (dropout_result == torch_dropout_result).sum() / dropout_result.size

    print("Dropout")
    print("Percentage of equal elements: {}".format(percentage_equal))

# check transposed features
check_transposed_features = False
if check_transposed_features:
    path = dir_path + "/features_T.npy"
    features_T = np.load(path)
    features_T_true = features.T

    print("features_T shape {}".format(features_T.shape))
    print("features_T_true shape {}".format(features_T_true.shape))

    assert(features_T.shape == features_T_true.shape)
    percentage_equal = (features_T == features_T_true).sum() / features_T_true.size

    print("Transposed features")
    print("Percentage of equal elements: {}".format(percentage_equal))

#check graph convolution
check_graph_conv = False
if check_graph_conv:
    path = dir_path + "/graph_conv_result.npy"
    graph_conv_result = np.load(path)
    # to row-major
    n, m = graph_conv_result.shape
    graph_conv_result = graph_conv_result.reshape((m, n))
    graph_conv_result = graph_conv_result.transpose()

    graph_conv_true_result = adj.dot(features)

    assert(graph_conv_result.shape == graph_conv_true_result.shape)
    percentage_equal = (graph_conv_result == graph_conv_true_result).sum() / graph_conv_true_result.size
    print("Graph convolution")
    print("Percentage of equal elements: {}".format(percentage_equal))

# check graph convolution with mean
check_graph_conv_mean = False
if check_graph_conv_mean:
    path = dir_path + "/graph_conv_mean_result.npy"
    graph_conv_mean_result = np.load(path)
    # to row-major
    n, m = graph_conv_mean_result.shape
    graph_conv_mean_result = graph_conv_mean_result.reshape((m, n))
    graph_conv_mean_result = graph_conv_mean_result.transpose()

    true_graph_conv_mean_result = adj.dot(features)
    true_sum = adj.dot(np.ones((adj.shape[1],)))
    for i in range(true_graph_conv_mean_result.shape[1]):
        true_graph_conv_mean_result[:, i] = true_graph_conv_mean_result[:, i] / true_sum

    assert(graph_conv_mean_result.shape == true_graph_conv_mean_result.shape)
    is_close = np.isclose(graph_conv_mean_result, true_graph_conv_mean_result)
    percentage_equal = is_close.sum() / true_graph_conv_mean_result.size
    print("Graph convolution with mean")
    print("Percentage of equal elements: {}".format(percentage_equal))

# check relu
check_relu = False
if check_relu:
    path = dir_path + "/relu_result.npy"
    relu_result = np.load(path)
    
    relu_layer = torch.nn.functional.relu
    features_torch = torch.from_numpy(features)
    true_relu_result = relu_layer(features_torch)
    true_relu_result = true_relu_result.numpy()

    assert(relu_result.shape == true_relu_result.shape)
    is_close = np.isclose(relu_result, true_relu_result)
    percentage_equal = is_close.sum() / true_relu_result.size
    print("ReLU")
    print("Percentage of equal elements: {}".format(percentage_equal))

# check sage linear
check_sage_linear = False
if check_sage_linear:
    path = dir_path + "/self_weight.npy"
    self_weight = np.load(path)
    path = dir_path + "/self_bias.npy"
    self_bias = np.load(path)
    path = dir_path + "/neigh_weight.npy"
    neigh_weight = np.load(path)
    path = dir_path + "/neigh_bias.npy"
    neigh_bias = np.load(path)

    path = dir_path + "/linear_in_features.npy"
    linear_in_features = np.load(path)
    path = dir_path + "/linear_in_aggregate.npy"
    linear_in_aggregate = np.load(path)
    path = dir_path + "/linear_result.npy"
    sage_linear_result = np.load(path)

    self_result = linear_in_features.dot(self_weight) + self_bias.T
    neigh_result = linear_in_aggregate.dot(neigh_weight) + neigh_bias.T
    true_sage_linear_result = self_result + neigh_result

    is_close = np.isclose(sage_linear_result, true_sage_linear_result)
    percentage_equal = is_close.sum() / true_sage_linear_result.size
    print("SageLinear")
    print("Percentage of equal elements: {}".format(percentage_equal))
    # matrices are column-major but the result is close

# check log-softmax
check_log_softmax = True
if check_log_softmax:
    path = dir_path + "/log_softmax_in.npy"
    log_softmax_in = np.load(path)
    path = dir_path + "/log_softmax_out.npy"
    log_softmax_out = np.load(path)
    # to row-major
    n, m = log_softmax_in.shape
    log_softmax_in = log_softmax_in.reshape((m, n))
    log_softmax_in = log_softmax_in.transpose()
    log_softmax_out = log_softmax_out.reshape((m, n))
    log_softmax_out = log_softmax_out.transpose()

    log_softmax_layer = torch.nn.LogSoftmax(dim=-1)
    true_log_softmax_out = log_softmax_layer(torch.from_numpy(log_softmax_in))
    true_log_softmax_out = true_log_softmax_out.numpy()

    print(log_softmax_in[0:10, 0:10])
    print("----")
    print(true_log_softmax_out[0:10, 0:10])
    print("----")
    print(log_softmax_layer(torch.from_numpy(log_softmax_in[0, 0:10])))
    print(log_softmax_layer(torch.from_numpy(log_softmax_in[1, 0:10])))

    is_close = np.isclose(log_softmax_out, true_log_softmax_out)
    percentage_equal = is_close.sum() / true_log_softmax_out.size
    print("Log-softmax")
    print("Percentage of equal elements: {}".format(percentage_equal))
