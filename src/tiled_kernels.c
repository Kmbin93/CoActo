#include "kernels.h"

#define _SKIP_KERNELS 0

void *prepare_input (ninst_t *ninst, void *buffer)
{
    nasm_ldata_t *ldata = ninst->ldata;
    aspen_layer_t *layer = ldata->layer;
    nasm_ldata_t *p_ldata = (ldata->parent_ldata_idx_arr[PARENT_0] + ldata->nasm->ldata_arr);
    aspen_layer_t *p_layer = (ldata->parent_ldata_idx_arr[PARENT_0] + ldata->nasm->ldata_arr)->layer;
    unsigned int parent_stride = p_ldata->out_mat_stride;
    void **input_ptr_arr = buffer;
    unsigned int num_idx = 0;
    if (layer->type == CONV_LAYER || layer->type == MAXPOOL_LAYER || layer->type == AVGPOOL_LAYER)
    {
        unsigned int mat_w = ninst->out_mat_pos[OUT_W];
        for (; mat_w < ninst->out_mat_pos[OUT_W] + ninst->tile_dims[OUT_W]; mat_w++)
        {
            unsigned int out_b = mat_w / (layer->params[OUT_H] * layer->params[OUT_W]); 
            unsigned int out_h = (mat_w % (layer->params[OUT_H] * layer->params[OUT_W])) / layer->params[OUT_W];
            unsigned int out_w = mat_w % layer->params[OUT_W];
            unsigned int in_b = out_b;
            for (int kh = 0; kh < layer->params[WEIGHT_H]; kh++)
            {
                for (int kw = 0; kw < layer->params[WEIGHT_W]; kw++)
                {
                    int in_h = out_h * layer->params[STRIDE] + kh  - layer->params[PADDING];
                    int in_w = out_w * layer->params[STRIDE] + kw  - layer->params[PADDING];
                    if (in_h < 0 || in_h >= p_layer->params[OUT_H] || in_w < 0 || in_w >= p_layer->params[OUT_W])
                    {
                        input_ptr_arr[num_idx++] = NULL;
                        continue;
                    }
                    unsigned int in_idx = in_b * p_layer->params[OUT_H] * p_layer->params[OUT_W] 
                        + in_h * p_layer->params[OUT_W] + in_w;
                    input_ptr_arr[num_idx++] = (char*)p_ldata->out_mat + in_idx*parent_stride * layer->dnn->element_size;
                }
            }
        }
    }
    else
    {
        FPRT(stderr, "ERROR: Unsupported layer type %s, at line %d in file %s\n" , layer_type_str[layer->type], __LINE__, __FILE__);
        assert (0);
    }
    return (void *) (input_ptr_arr + num_idx);
}

void tiled_conv2d (ninst_t *ninst, ase_t *ase)
{
    #if _SKIP_KERNELS == 0
    nasm_ldata_t *ldata = ninst->ldata;
    aspen_layer_t *layer = ninst->ldata->layer;
    nasm_ldata_t *p_ldata = (ldata->parent_ldata_idx_arr[PARENT_0] + ldata->nasm->ldata_arr);
    // void *scratchpad = prepare_input (ninst, ase->scratchpad);
    unsigned int input_col_size = p_ldata->out_mat_dims[OUT_H];
    // void **input_ptr_arr = ase->scratchpad;   
    char *input = ase->scratchpad; // (char *) scratchpad;
    const unsigned int input_pos_per_n = ninst->num_input_pos/ninst->tile_dims[OUT_W];
    const unsigned int M = ninst->tile_dims[OUT_H];
    const unsigned int N = ninst->tile_dims[OUT_W];
    const unsigned int K = layer->params[WEIGHT_H] * layer->params[WEIGHT_W] * layer->params[IN_C];
    const unsigned int lda = K;
    const unsigned int ldb = K;
    const unsigned int ldc = ldata->out_mat_stride;
    const void *A = (char*)layer->tensors[WEIGHT_TENSOR]->data + ninst->out_mat_pos[OUT_H] * lda * layer->dnn->element_size;
    const void *B = ase->scratchpad;
    void *C = ninst->out_mat;
    const unsigned int rem_n = N % _TILE_SIZE_N;
    const unsigned int rem_m = M % _TILE_SIZE_M;
    const unsigned int rem_k = K % _TILE_SIZE_K;
    unsigned int n = 0;
    // <M, N, K> = <M, _TILE_SIZE_N, K>
    for (; n < N - rem_n; n += _TILE_SIZE_N)
    {
        for (unsigned int i = input_pos_per_n * n; i < input_pos_per_n * (n + _TILE_SIZE_N); i++)
        {
            void *input_ptr = ninst->input_pos_idx_arr[i]*p_ldata->out_mat_stride + (float *) p_ldata->out_mat;
            if (ninst->input_pos_idx_arr[i] == -1)
            {
                memset (input, 0, input_col_size * layer->dnn->element_size);
            }
            else
            {
                memcpy (input, input_ptr, input_col_size * layer->dnn->element_size);
            }
            input += input_col_size * layer->dnn->element_size;
        }
        unsigned int k = 0;
        // <M, N, K> = <M, _TILE_SIZE_N, _TILE_SIZE_K>
        for (; k < K - rem_k; k += _TILE_SIZE_K)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, _TILE_SIZE_N, _TILE_SIZE_K>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_FULL_TILE (_TILE_SIZE_M, _TILE_SIZE_N, _TILE_SIZE_K, 
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, _TILE_SIZE_N, _TILE_SIZE_K>
            if (rem_m != 0)
                SGEMM_KERNEL_TILE_N (rem_m, _TILE_SIZE_N, _TILE_SIZE_K, 
                        (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        
        }
        // <M, N, K> = <M, _TILE_SIZE_N, rem_k>
        if (rem_k != 0)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, _TILE_SIZE_N, rem_k>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_FULL_TILE (_TILE_SIZE_M, _TILE_SIZE_N, rem_k,
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, _TILE_SIZE_N, rem_k>
            SGEMM_KERNEL_TILE_N (rem_m, _TILE_SIZE_N, rem_k, 
                (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        }
        for (unsigned int nn = n; nn < n + _TILE_SIZE_N; nn++)
        {
            float *out_vec = (float *)C + nn * ldc;
            if (layer->tensors[BIAS_TENSOR] != NULL)
            {
                float *bias = (float*)layer->tensors[BIAS_TENSOR]->data + ninst->out_mat_pos[OUT_H];
                for (unsigned int m = 0; m < M; m++)
                {
                    out_vec[m] += bias[m];
                }
            }
            naive_activate (out_vec, M, layer->activation);
        }
    }
    // <M, N, K> = <M, rem_n, K>
    if (rem_n != 0)
    {
        for (unsigned int i = input_pos_per_n * n; i < input_pos_per_n * N; i++)
        {
            void *input_ptr = ninst->input_pos_idx_arr[i]*p_ldata->out_mat_stride + (float *) p_ldata->out_mat;
                if (ninst->input_pos_idx_arr[i] == -1)
            {
                memset (input, 0, input_col_size * layer->dnn->element_size);
            }
            else
            {
                memcpy (input, input_ptr, input_col_size * layer->dnn->element_size);
            }
            input += input_col_size * layer->dnn->element_size;
        }
        unsigned int k = 0;
        // <M, N, K> = <M, rem_n, _TILE_SIZE_K>
        for (; k < K - rem_k; k += _TILE_SIZE_K)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, rem_n, _TILE_SIZE_K>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_TILE_M (_TILE_SIZE_M, rem_n, _TILE_SIZE_K, 
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, rem_n, _TILE_SIZE_K>
            if (rem_m != 0)
                SGEMM_KERNEL (rem_m, rem_n, _TILE_SIZE_K, 
                        (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        }
        // <M, N, K> = <M, rem_n, rem_k>
        if (rem_k != 0)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, rem_n, rem_k>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_TILE_M (_TILE_SIZE_M, rem_n, rem_k,
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, rem_n, rem_k>
            SGEMM_KERNEL (rem_m, rem_n, rem_k, 
                (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        }
        for (unsigned int nn = n; nn < N; nn++)
        {
            float *out_vec = (float *)C + nn * ldc;
            if (layer->tensors[BIAS_TENSOR] != NULL)
            {
                float *bias = (float*)layer->tensors[BIAS_TENSOR]->data + ninst->out_mat_pos[OUT_H];
                for (unsigned int m = 0; m < M; m++)
                {
                    out_vec[m] += bias[m];
                }
            }
            naive_activate (out_vec, M, layer->activation);
        }
    }
    #endif
}

void tiled_matmul (ninst_t *ninst, ase_t *ase)
{
    #if _SKIP_KERNELS == 0
    nasm_ldata_t *ldata = ninst->ldata;
    aspen_layer_t *layer = ninst->ldata->layer;
    nasm_ldata_t *p_ldata = (ldata->parent_ldata_idx_arr[PARENT_0] + ldata->nasm->ldata_arr);
    const unsigned int M = ninst->tile_dims[OUT_H];
    const unsigned int N = ninst->tile_dims[OUT_W];
    const unsigned int K = layer->params[MAT_K];
    const unsigned int lda = K;
    const unsigned int ldb = p_ldata->out_mat_stride;
    const unsigned int ldc = ldata->out_mat_stride;
    const void *A = (char*)layer->tensors[WEIGHT_TENSOR]->data + (ninst->out_mat_pos[OUT_H] * lda * layer->dnn->element_size);
    const void *B = (char*)p_ldata->out_mat + (ninst->out_mat_pos[OUT_W] * ldb * layer->dnn->element_size);
    void *C = ninst->out_mat;
    const unsigned int rem_n = N % _TILE_SIZE_N;
    const unsigned int rem_m = M % _TILE_SIZE_M;
    const unsigned int rem_k = K % _TILE_SIZE_K;
    unsigned int n = 0;

    // <M, N, K> = <M, _TILE_SIZE_N, K>
    for (; n < N - rem_n; n += _TILE_SIZE_N)
    {
        unsigned int k = 0;
        // <M, N, K> = <M, _TILE_SIZE_N, _TILE_SIZE_K>
        for (; k < K - rem_k; k += _TILE_SIZE_K)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, _TILE_SIZE_N, _TILE_SIZE_K>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_FULL_TILE (_TILE_SIZE_M, _TILE_SIZE_N, _TILE_SIZE_K, 
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, _TILE_SIZE_N, _TILE_SIZE_K>
            if (rem_m != 0)
                SGEMM_KERNEL_TILE_N (rem_m, _TILE_SIZE_N, _TILE_SIZE_K, 
                        (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        
        }
        // <M, N, K> = <M, _TILE_SIZE_N, rem_k>
        if (rem_k != 0)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, _TILE_SIZE_N, rem_k>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_FULL_TILE (_TILE_SIZE_M, _TILE_SIZE_N, rem_k,
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, _TILE_SIZE_N, rem_k>
            SGEMM_KERNEL_TILE_N (rem_m, _TILE_SIZE_N, rem_k, 
                (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        }
        for (unsigned int nn = n; nn < n + _TILE_SIZE_N; nn++)
        {
            float *out_vec = (float *)C + nn * ldc;
            if (layer->tensors[BIAS_TENSOR] != NULL)
            {
                float *bias = (float*)layer->tensors[BIAS_TENSOR]->data + ninst->out_mat_pos[OUT_H];
                for (unsigned int m = 0; m < M; m++)
                {
                    out_vec[m] += bias[m];
                }
            }
            naive_activate (out_vec, M, layer->activation);
        }
    }
    // <M, N, K> = <M, rem_n, K>
    if (rem_n != 0)
    {
        unsigned int k = 0;
        // <M, N, K> = <M, rem_n, _TILE_SIZE_K>
        for (; k < K - rem_k; k += _TILE_SIZE_K)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, rem_n, _TILE_SIZE_K>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_TILE_M (_TILE_SIZE_M, rem_n, _TILE_SIZE_K, 
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, rem_n, _TILE_SIZE_K>
            if (rem_m != 0)
                SGEMM_KERNEL (rem_m, rem_n, _TILE_SIZE_K, 
                        (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        }
        // <M, N, K> = <M, rem_n, rem_k>
        if (rem_k != 0)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, rem_n, rem_k>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_TILE_M (_TILE_SIZE_M, rem_n, rem_k,
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, rem_n, rem_k>
            SGEMM_KERNEL (rem_m, rem_n, rem_k, 
                (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        }
        for (unsigned int nn = n; nn < N; nn++)
        {
            float *out_vec = (float *)C + nn * ldc;
            if (layer->tensors[BIAS_TENSOR] != NULL)
            {
                float *bias = (float*)layer->tensors[BIAS_TENSOR]->data + ninst->out_mat_pos[OUT_H];
                for (unsigned int m = 0; m < M; m++)
                {
                    out_vec[m] += bias[m];
                }
            }
            naive_activate (out_vec, M, layer->activation);
        }
    }
    #endif
}

void tiled_maxpool2d (ninst_t *ninst, ase_t *ase)
{
    #if _SKIP_KERNELS == 0
    nasm_ldata_t *ldata = ninst->ldata;
    aspen_layer_t *layer = ninst->ldata->layer;
    prepare_input (ninst, ase->scratchpad);
    void **input_ptr_arr = ase->scratchpad;   
    const unsigned int input_pos_per_n = ninst->num_input_pos/ninst->tile_dims[OUT_W];
    const unsigned int M = ninst->tile_dims[OUT_H];
    const unsigned int N = ninst->tile_dims[OUT_W];
    const unsigned int ldc = ldata->out_mat_stride;
    void *C = ninst->out_mat;
    for (int n = 0; n < N; n++)
    {
        float *out_vec = (float*)C + n * ldc;
        float *input_vec = (float *)input_ptr_arr[n*input_pos_per_n];
        if (input_vec == NULL)
        {
            for (int m = 0; m < M; m++)
            {
                out_vec[m] = -INFINITY;
            }
        }
        else
        {
            input_vec += ninst->out_mat_pos[OUT_H];
            memcpy (out_vec, input_vec, M * layer->dnn->element_size);
        }
        for (int i = 1; i < input_pos_per_n; i++)
        {
            input_vec = (float *)input_ptr_arr[n*input_pos_per_n + i];
            if (input_vec != NULL)
            {
                input_vec += + ninst->out_mat_pos[OUT_H];
                for (int m = 0; m < M; m++)
                {
                    out_vec[m] = out_vec[m] >= input_vec[m] ? out_vec[m] : input_vec[m];
                }
            }
        }
        naive_activate (out_vec, M, layer->activation);
    }
    #endif
}
void tiled_avgpool2d (ninst_t *ninst, ase_t *ase)
{
    #if _SKIP_KERNELS == 0
    nasm_ldata_t *ldata = ninst->ldata;
    aspen_layer_t *layer = ninst->ldata->layer;
    prepare_input (ninst, ase->scratchpad);
    void **input_ptr_arr = ase->scratchpad;   
    const unsigned int input_pos_per_n = ninst->num_input_pos/ninst->tile_dims[OUT_W];
    const unsigned int M = ninst->tile_dims[OUT_H];
    const unsigned int N = ninst->tile_dims[OUT_W];
    const unsigned int ldc = ldata->out_mat_stride;
    void *C = ninst->out_mat;
    for (int n = 0; n < N; n++)
    {
        float *out_vec = (float*)C + n * ldc;
        float *input_vec = (float*)input_ptr_arr[n*input_pos_per_n];
        if (input_vec == NULL)
        {
            memset (out_vec, 0, M * layer->dnn->element_size);
        }
        else
        {
            input_vec += ninst->out_mat_pos[OUT_H];
            memcpy (out_vec, input_vec, M * layer->dnn->element_size);
        }
        for (int i = 1; i < input_pos_per_n; i++)
        {
            input_vec = (float*)input_ptr_arr[n*input_pos_per_n + i];
            if (input_vec != NULL)
            {
                input_vec += ninst->out_mat_pos[OUT_H];
                for (int m = 0; m < M; m++)
                {
                    out_vec[m] += input_vec[m];
                }
            }
        }
        for (int m = 0; m < M; m++)
        {
            out_vec[m] /= input_pos_per_n;
        }
        naive_activate (out_vec, M, layer->activation);
    }
    #endif
}
void tiled_fully_connected (ninst_t *ninst, ase_t *ase)
{
    

}
void tiled_residual (ninst_t *ninst, ase_t *ase)
{
    #if _SKIP_KERNELS == 0
    nasm_ldata_t *ldata = ninst->ldata;
    aspen_layer_t *layer = ninst->ldata->layer;
    nasm_ldata_t *p0_ldata = (ldata->parent_ldata_idx_arr[PARENT_0] + ldata->nasm->ldata_arr);
    nasm_ldata_t *p1_ldata = (ldata->parent_ldata_idx_arr[PARENT_1] + ldata->nasm->ldata_arr);
    const unsigned int M = ninst->tile_dims[OUT_H];
    const unsigned int N = ninst->tile_dims[OUT_W];
    const unsigned int ldc = ldata->out_mat_stride;
    void *C = ninst->out_mat;
    for (int n = 0; n < N; n++)
    {
        unsigned int w_pos = ninst->out_mat_pos[OUT_W] + n;
        float *input_0 = (float*)p0_ldata->out_mat + w_pos * p0_ldata->out_mat_stride + ninst->out_mat_pos[OUT_H];
        float *input_1 = (float*)p1_ldata->out_mat + w_pos * p1_ldata->out_mat_stride + ninst->out_mat_pos[OUT_H];
        float *out_vec = (float*)C + n * ldc;
        for (int m = 0; m < M; m++)
        {
            out_vec[m] = input_0[m] + input_1[m];
        }
        naive_activate (out_vec, M, layer->activation);
    }
    #endif
}

void tiled_softmax (ninst_t *ninst, ase_t *ase)
{
    #if _SKIP_KERNELS == 0
    nasm_ldata_t *ldata = ninst->ldata;
    nasm_ldata_t *p0_ldata = (ldata->parent_ldata_idx_arr[PARENT_0] + ldata->nasm->ldata_arr);
    const unsigned int M = ninst->tile_dims[OUT_H];
    const unsigned int N = ninst->tile_dims[OUT_W];
    const unsigned int ldc = ldata->out_mat_stride;
    void *C = ninst->out_mat;
    for (int n = 0; n < N; n++)
    {
        unsigned int w_pos = ninst->out_mat_pos[OUT_W] + n;
        float *input = (float*)p0_ldata->out_mat + w_pos * p0_ldata->out_mat_stride + ninst->out_mat_pos[OUT_H];
        float *output = (float*)C + n * ldc;
        float max = input[0];
        for (int m = 1; m < M; m++)
        {
            max = max >= input[m] ? max : input[m];
        }
        float sum = 0;
        for (int m = 0; m < M; m++)
        {
            output[m] = expf (input[m] - max);
            sum += output[m];
        }
        for (int m = 0; m < M; m++)
        {
            output[m] /= sum;
        }
    }
    #endif
}

void tiled_layernorm (ninst_t *ninst, ase_t *ase)
{
    #if _SKIP_KERNELS == 0
    nasm_ldata_t *ldata = ninst->ldata;
    aspen_layer_t *layer = ninst->ldata->layer;
    nasm_ldata_t *p_ldata = (ldata->parent_ldata_idx_arr[PARENT_0] + ldata->nasm->ldata_arr);
    const unsigned int M = ninst->tile_dims[OUT_H];
    const unsigned int N = ninst->tile_dims[OUT_W];
    const unsigned int ldb = p_ldata->out_mat_stride;
    const unsigned int ldc = ldata->out_mat_stride;
    const float *weight = (float*)layer->tensors[WEIGHT_TENSOR]->data + ninst->out_mat_pos[OUT_H];
    const float *bias = NULL;
    if (layer->tensors[BIAS_TENSOR] != NULL)
        bias = (float*)layer->tensors[BIAS_TENSOR]->data + ninst->out_mat_pos[OUT_H];
    const void *B = (char*)p_ldata->out_mat + (ninst->out_mat_pos[OUT_W] * ldb * layer->dnn->element_size);
    void *C = ninst->out_mat;

    for (int n = 0; n < N; n++)
    {
        const float *input_vec = (float*)B + n * ldb;
        float *out_vec = (float*)C + n * ldc;
        float mean = 0;
        float var = 0;
        for (int m = 0; m < M; m++)
        {
            mean += input_vec[m];
            var += input_vec[m] * input_vec[m];
        }
        mean /= M;
        var /= M;
        var -= mean * mean;
        var = 1 / sqrtf (var + 1e-6f);
        for (int m = 0; m < M; m++)
        {
            out_vec[m] = (input_vec[m] - mean) * var * weight[m];
            if (bias != NULL)
                out_vec[m] += bias[m];
        }
    }

    #endif
}
void tiled_k_attention (ninst_t *ninst, ase_t *ase)
{
    #if _SKIP_KERNELS == 0
    nasm_ldata_t *ldata = ninst->ldata;
    aspen_layer_t *layer = ninst->ldata->layer;
    nasm_ldata_t *p_ldata = (ldata->parent_ldata_idx_arr[PARENT_0] + ldata->nasm->ldata_arr);
    nasm_ldata_t *pk_ldata = (ldata->parent_ldata_idx_arr[PARENT_1] + ldata->nasm->ldata_arr);
    const unsigned int num_hidden = layer->params[NUM_HIDDEN];
    const unsigned int num_heads = layer->params[NUM_HEAD];
    const unsigned int hidden_per_head = num_hidden / num_heads;
    const unsigned int num_seq = ldata->nasm->tr_seq_len;
    const unsigned int batch = ninst->out_mat_pos[OUT_W] / (num_seq * num_heads);
    const unsigned int head = (ninst->out_mat_pos[OUT_W] % (num_seq * num_heads)) / num_seq;
    const unsigned int M = ninst->tile_dims[OUT_H];
    const unsigned int N = ninst->tile_dims[OUT_W];
    const unsigned int K = layer->params[MAT_K];
    const unsigned int ldk = pk_ldata->out_mat_stride;
    const unsigned int lda = K;
    const unsigned int ldb = p_ldata->out_mat_stride;
    const unsigned int ldc = ldata->out_mat_stride;
    void *A = ase->scratchpad;
    const void *B = (char*)p_ldata->out_mat + (batch * num_hidden * num_seq + head * hidden_per_head +
        + (ninst->out_mat_pos[OUT_W] % num_seq) * ldb) * layer->dnn->element_size;
    void *C = ninst->out_mat;
    const unsigned int rem_n = N % _TILE_SIZE_N;
    const unsigned int rem_m = M % _TILE_SIZE_M;
    const unsigned int rem_k = K % _TILE_SIZE_K;
    unsigned int n = 0;

    // Transpose & Reoder Key data.
    const float *key_head = (float*)pk_ldata->out_mat + batch * ldk * num_seq + head * hidden_per_head;
    for (unsigned int m = 0; m < M; m++)
    {
        for (unsigned int k = 0; k < K; k++)
        {
            const float* input_ptr = key_head + (ninst->out_mat_pos[OUT_H] + m) 
                * ldk + k;
            float* output_ptr = (float*)A + ((m/_VEC_SIZE_M) * lda + k) * _VEC_SIZE_M 
                + (m % _VEC_SIZE_M);
            *output_ptr = *input_ptr;
        }
    }
    // <M, N, K> = <M, _TILE_SIZE_N, K>
    for (; n < N - rem_n; n += _TILE_SIZE_N)
    {
        unsigned int k = 0;
        // <M, N, K> = <M, _TILE_SIZE_N, _TILE_SIZE_K>
        for (; k < K - rem_k; k += _TILE_SIZE_K)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, _TILE_SIZE_N, _TILE_SIZE_K>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_FULL_TILE (_TILE_SIZE_M, _TILE_SIZE_N, _TILE_SIZE_K, 
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, _TILE_SIZE_N, _TILE_SIZE_K>
            if (rem_m != 0)
                SGEMM_KERNEL_TILE_N (rem_m, _TILE_SIZE_N, _TILE_SIZE_K, 
                        (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        
        }
        // <M, N, K> = <M, _TILE_SIZE_N, rem_k>
        if (rem_k != 0)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, _TILE_SIZE_N, rem_k>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_FULL_TILE (_TILE_SIZE_M, _TILE_SIZE_N, rem_k,
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, _TILE_SIZE_N, rem_k>
            SGEMM_KERNEL_TILE_N (rem_m, _TILE_SIZE_N, rem_k, 
                (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        }
        for (unsigned int nn = n; nn < n + _TILE_SIZE_N; nn++)
        {
            float total = 0;
            for (unsigned int j = 0; j < M; j++)
            {
                float val = *((float*)C + nn*ldc + j);
                val /= sqrtf (hidden_per_head);
                *((float*)C + nn*ldc + j) = expf (val);
                total += *((float*)C + nn*ldc + j);
            }
            for (unsigned int j = 0; j < M; j++)
                *((float*)C + nn*ldc + j) /= total;
        }
    }
    // <M, N, K> = <M, rem_n, K>
    if (rem_n != 0)
    {
        unsigned int k = 0;
        // <M, N, K> = <M, rem_n, _TILE_SIZE_K>
        for (; k < K - rem_k; k += _TILE_SIZE_K)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, rem_n, _TILE_SIZE_K>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_TILE_M (_TILE_SIZE_M, rem_n, _TILE_SIZE_K, 
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, rem_n, _TILE_SIZE_K>
            if (rem_m != 0)
                SGEMM_KERNEL (rem_m, rem_n, _TILE_SIZE_K, 
                        (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        }
        // <M, N, K> = <M, rem_n, rem_k>
        if (rem_k != 0)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, rem_n, rem_k>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_TILE_M (_TILE_SIZE_M, rem_n, rem_k,
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, rem_n, rem_k>
            SGEMM_KERNEL (rem_m, rem_n, rem_k, 
                (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        }
        for (unsigned int nn = n; nn < N; nn++)
        {
            float total = 0;
            for (unsigned int j = 0; j < M; j++)
            {
                float val = *((float*)C + nn*ldc + j);
                val /= sqrtf (hidden_per_head);
                *((float*)C + nn*ldc + j) = expf (val);
                total += *((float*)C + nn*ldc + j);
            }
            for (unsigned int j = 0; j < M; j++)
                *((float*)C + nn*ldc + j) /= total;
        }
    }
    #endif
}

void tiled_v_attention (ninst_t *ninst, ase_t *ase)
{
    #if _SKIP_KERNELS == 0
    nasm_ldata_t *ldata = ninst->ldata;
    aspen_layer_t *layer = ninst->ldata->layer;
    nasm_ldata_t *p_ldata = (ldata->parent_ldata_idx_arr[PARENT_0] + ldata->nasm->ldata_arr);
    nasm_ldata_t *pv_ldata = (ldata->parent_ldata_idx_arr[PARENT_1] + ldata->nasm->ldata_arr);
    const unsigned int num_hidden = layer->params[NUM_HIDDEN];
    const unsigned int num_heads = layer->params[NUM_HEAD];
    const unsigned int hidden_per_head = num_hidden / num_heads;
    const unsigned int num_seq = ldata->nasm->tr_seq_len;
    unsigned int batch = ninst->out_mat_pos[OUT_W] / num_seq;
    const unsigned int head = ninst->out_mat_pos[OUT_H]  / hidden_per_head;
    const unsigned int M = ninst->tile_dims[OUT_H];
    const unsigned int N = ninst->tile_dims[OUT_W];
    const unsigned int K = num_seq;
    const unsigned int ldv = pv_ldata->out_mat_stride;
    const unsigned int lda = K;
    const unsigned int ldb = p_ldata->out_mat_stride;
    const unsigned int ldc = ldata->out_mat_stride;
    void *A = ase->scratchpad;
    const void *B = (float*)p_ldata->out_mat + (batch * num_heads * num_seq + head * num_seq +
        + (ninst->out_mat_pos[OUT_W] % num_seq)) * ldb;
    void *C = ninst->out_mat;
    const unsigned int rem_n = N % _TILE_SIZE_N;
    const unsigned int rem_m = M % _TILE_SIZE_M;
    const unsigned int rem_k = K % _TILE_SIZE_K;
    unsigned int n = 0;

    // Transpose & Reoder Value data.
    const float *in_val_ptr = (float*)pv_ldata->out_mat + batch * ldv * num_seq;
    for (unsigned int m = 0; m < M; m++)
    {
        for (unsigned int k = 0; k < K; k++)
        {
            const float* input_ptr = in_val_ptr + k * ldv + (ninst->out_mat_pos[OUT_H] + m);
            float* output_ptr = (float*)A + ((m/_VEC_SIZE_M) * lda + k) * _VEC_SIZE_M 
                + (m % _VEC_SIZE_M);
            *output_ptr = *input_ptr;
        }
    }
    // <M, N, K> = <M, _TILE_SIZE_N, K>
    for (; n < N - rem_n; n += _TILE_SIZE_N)
    {
        unsigned int k = 0;
        // <M, N, K> = <M, _TILE_SIZE_N, _TILE_SIZE_K>
        for (; k < K - rem_k; k += _TILE_SIZE_K)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, _TILE_SIZE_N, _TILE_SIZE_K>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_FULL_TILE (_TILE_SIZE_M, _TILE_SIZE_N, _TILE_SIZE_K, 
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, _TILE_SIZE_N, _TILE_SIZE_K>
            if (rem_m != 0)
                SGEMM_KERNEL_TILE_N (rem_m, _TILE_SIZE_N, _TILE_SIZE_K, 
                        (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        
        }
        // <M, N, K> = <M, _TILE_SIZE_N, rem_k>
        if (rem_k != 0)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, _TILE_SIZE_N, rem_k>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_FULL_TILE (_TILE_SIZE_M, _TILE_SIZE_N, rem_k,
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, _TILE_SIZE_N, rem_k>
            SGEMM_KERNEL_TILE_N (rem_m, _TILE_SIZE_N, rem_k, 
                (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        }
    }
    // <M, N, K> = <M, rem_n, K>
    if (rem_n != 0)
    {
        unsigned int k = 0;
        // <M, N, K> = <M, rem_n, _TILE_SIZE_K>
        for (; k < K - rem_k; k += _TILE_SIZE_K)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, rem_n, _TILE_SIZE_K>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_TILE_M (_TILE_SIZE_M, rem_n, _TILE_SIZE_K, 
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, rem_n, _TILE_SIZE_K>
            if (rem_m != 0)
                SGEMM_KERNEL (rem_m, rem_n, _TILE_SIZE_K, 
                        (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        }
        // <M, N, K> = <M, rem_n, rem_k>
        if (rem_k != 0)
        {
            unsigned int m = 0;
            // <M, N, K> = <_TILE_SIZE_M, rem_n, rem_k>
            for (; m < M - rem_m; m += _TILE_SIZE_M)
            {
                SGEMM_KERNEL_TILE_M (_TILE_SIZE_M, rem_n, rem_k,
                    (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
            }
            // <M, N, K> = <rem_m, rem_n, rem_k>
            SGEMM_KERNEL (rem_m, rem_n, rem_k, 
                (float*)A + (m * lda + k*_VEC_SIZE_M), lda, (float*)B + (n * ldb + k), ldb, (float*)C + (ldc * n + m), ldc);
        }
    }
    #endif
}