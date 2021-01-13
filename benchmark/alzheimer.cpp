// Copyright 2020 Marcel Wagenländer

#include "alzheimer.hpp"
#include "dataset.hpp"
#include "gpu_memory_logger.hpp"

#include <benchmark/benchmark.h>
#include <string>


void benchmark_alzheimer(Dataset dataset, benchmark::State &state) {
    GPUMemoryLogger memory_logger("alzheimer_" + get_dataset_name(dataset));

    memory_logger.start();

    for (auto _ : state)
        alzheimer(dataset);

    memory_logger.stop();
}

void benchmark_alzheimer_chunked(Dataset dataset, benchmark::State &state) {
    GPUMemoryLogger memory_logger("alzheimer_chunked_" + get_dataset_name(dataset) + "_" + std::to_string(state.range(0)));
    memory_logger.start();

    for (auto _ : state)
        alzheimer_chunked(dataset, state.range(0));

    memory_logger.stop();
}

void benchmark_alzheimer_pipelined(Dataset dataset, benchmark::State &state) {
    GPUMemoryLogger memory_logger("alzheimer_pipelined_" + get_dataset_name(dataset) + "_" + std::to_string(state.range(0)));
    memory_logger.start();

    for (auto _ : state)
        alzheimer_pipelined(dataset, state.range(0));

    memory_logger.stop();
}

// LAYER --- LAYER --- LAYER

static void BM_Alzheimer_Layer_Flickr(benchmark::State &state) {
    benchmark_alzheimer(flickr, state);
}
BENCHMARK(BM_Alzheimer_Layer_Flickr);

static void BM_Alzheimer_Layer_Reddit(benchmark::State &state) {
    benchmark_alzheimer(reddit, state);
}
BENCHMARK(BM_Alzheimer_Layer_Reddit);

static void BM_Alzheimer_Layer_Products(benchmark::State &state) {
    benchmark_alzheimer(products, state);
}
BENCHMARK(BM_Alzheimer_Layer_Products);

// CHUNKED --- CHUNKED --- CHUNKED

static void BM_Alzheimer_Chunked_Flickr(benchmark::State &state) {
    benchmark_alzheimer_chunked(flickr, state);
}
BENCHMARK(BM_Alzheimer_Chunked_Flickr)->RangeMultiplier(2)->Range(1 << 14, 1 << 16);

static void BM_Alzheimer_Chunked_Reddit(benchmark::State &state) {
    benchmark_alzheimer_chunked(reddit, state);
}
BENCHMARK(BM_Alzheimer_Chunked_Reddit)->RangeMultiplier(2)->Range(1 << 14, 1 << 17);

static void BM_Alzheimer_Chunked_Products(benchmark::State &state) {
    benchmark_alzheimer_chunked(products, state);
}
BENCHMARK(BM_Alzheimer_Chunked_Products)->RangeMultiplier(2)->Range(1 << 14, 1 << 21);

static void BM_Alzheimer_Chunked_Ivy(benchmark::State &state) {
    benchmark_alzheimer_chunked(ivy, state);
}
BENCHMARK(BM_Alzheimer_Chunked_Ivy)->RangeMultiplier(2)->Range(1 << 14, 1 << 20);

// largest chunk size possible

static void BM_Alzheimer_Chunked_Flickr_Max(benchmark::State &state) {
    benchmark_alzheimer_chunked(flickr, state);
}
BENCHMARK(BM_Alzheimer_Chunked_Flickr_Max)->Arg(89250);

static void BM_Alzheimer_Chunked_Reddit_Max(benchmark::State &state) {
    benchmark_alzheimer_chunked(reddit, state);
}
BENCHMARK(BM_Alzheimer_Chunked_Reddit_Max)->Arg(232965);

static void BM_Alzheimer_Chunked_Products_Max(benchmark::State &state) {
    benchmark_alzheimer_chunked(products, state);
}
BENCHMARK(BM_Alzheimer_Chunked_Products_Max)->Arg(2449029);

static void BM_Alzheimer_Chunked_Ivy_Max(benchmark::State &state) {
    benchmark_alzheimer_chunked(ivy, state);
}
BENCHMARK(BM_Alzheimer_Chunked_Ivy_Max)->Arg(1371507);

// largest chunk size possible

static void BM_Alzheimer_Chunked_Flickr_X(benchmark::State &state) {
    benchmark_alzheimer_chunked(flickr, state);
}
BENCHMARK(BM_Alzheimer_Chunked_Flickr_X)->Arg(2 << 14);

static void BM_Alzheimer_Chunked_Reddit_X(benchmark::State &state) {
    benchmark_alzheimer_chunked(reddit, state);
}
BENCHMARK(BM_Alzheimer_Chunked_Reddit_X)->Arg(2 << 16);

static void BM_Alzheimer_Chunked_Products_X(benchmark::State &state) {
    benchmark_alzheimer_chunked(products, state);
}
BENCHMARK(BM_Alzheimer_Chunked_Products_X)->Arg(2 << 19);

static void BM_Alzheimer_Chunked_Ivy_X(benchmark::State &state) {
    benchmark_alzheimer_chunked(ivy, state);
}
BENCHMARK(BM_Alzheimer_Chunked_Ivy_X)->Arg(1371507);

// PIPELINED --- PIPELINED --- PIPELINED

static void BM_Alzheimer_Pipelined_Flickr(benchmark::State &state) {
    benchmark_alzheimer_pipelined(flickr, state);
}
BENCHMARK(BM_Alzheimer_Pipelined_Flickr)->RangeMultiplier(2)->Range(1 << 14, 1 << 16);

static void BM_Alzheimer_Pipelined_Reddit(benchmark::State &state) {
    benchmark_alzheimer_pipelined(reddit, state);
}
BENCHMARK(BM_Alzheimer_Pipelined_Reddit)->RangeMultiplier(2)->Range(1 << 14, 1 << 17);

static void BM_Alzheimer_Pipelined_Products(benchmark::State &state) {
    benchmark_alzheimer_pipelined(products, state);
}
BENCHMARK(BM_Alzheimer_Pipelined_Products)->RangeMultiplier(2)->Range(1 << 14, 1 << 20);

static void BM_Alzheimer_Pipelined_Ivy(benchmark::State &state) {
    benchmark_alzheimer_pipelined(ivy, state);
}
BENCHMARK(BM_Alzheimer_Pipelined_Ivy)->RangeMultiplier(2)->Range(1 << 14, 1 << 19);
