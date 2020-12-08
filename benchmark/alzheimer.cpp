// Copyright 2020 Marcel Wagenländer

#include "alzheimer.hpp"
#include "gpu_memory_logger.hpp"

#include <benchmark/benchmark.h>
#include <string>


static void BM_Alzheimer_Flickr(benchmark::State &state) {
    std::string dataset = "flickr";

    GPUMemoryLogger memory_logger("alzheimer_" + dataset);

    memory_logger.start();

    for (auto _ : state)
        alzheimer(dataset);

    memory_logger.stop();
}
BENCHMARK(BM_Alzheimer_Flickr);

static void BM_Alzheimer_Flickr_Chunked(benchmark::State &state) {
    std::string dataset = "flickr";

    GPUMemoryLogger memory_logger("alzheimer_" + dataset + "_" + std::to_string(state.range(0)));
    memory_logger.start();

    for (auto _ : state)
        alzheimer_chunked(dataset, state.range(0));

    memory_logger.stop();
}
BENCHMARK(BM_Alzheimer_Flickr_Chunked)->Range(1 << 10, 1 << 15);


static void BM_Alzheimer_Reddit(benchmark::State &state) {
    std::string dataset = "reddit";

    GPUMemoryLogger memory_logger("alzheimer_" + dataset);

    memory_logger.start();

    for (auto _ : state)
        alzheimer(dataset);

    memory_logger.stop();
}
BENCHMARK(BM_Alzheimer_Reddit);

static void BM_Alzheimer_Chunked_Reddit(benchmark::State &state) {
    std::string dataset = "reddit";

    GPUMemoryLogger memory_logger("alzheimer_" + dataset + "_" + std::to_string(state.range(0)));
    memory_logger.start();

    for (auto _ : state)
        alzheimer_chunked(dataset, state.range(0));

    memory_logger.stop();
}
BENCHMARK(BM_Alzheimer_Chunked_Reddit)->Range(1 << 12, 1 << 17);

static void BM_Alzheimer_Products(benchmark::State &state) {
    std::string dataset = "products";

    GPUMemoryLogger memory_logger("alzheimer_" + dataset);

    memory_logger.start();

    for (auto _ : state)
        alzheimer(dataset);

    memory_logger.stop();
}
BENCHMARK(BM_Alzheimer_Products);

static void BM_Alzheimer_Chunked_Products(benchmark::State &state) {
    std::string dataset = "products";

    GPUMemoryLogger memory_logger("alzheimer_" + dataset + "_" + std::to_string(state.range(0)));
    memory_logger.start();

    for (auto _ : state)
        alzheimer_chunked(dataset, state.range(0));

    memory_logger.stop();
}
BENCHMARK(BM_Alzheimer_Chunked_Products)->Range(1 << 16, 1 << 21);
