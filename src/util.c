#include "util.h"

char *ninst_state_str[NUM_NINST_STATES] = 
{
    [NINST_NOT_READY] = "NINST_NOT_READY", [NINST_READY] = "NINST_READY", [NINST_COMPLETED] = "NINST_COMPLETED"
};

char *layer_type_str [NUM_LAYER_ELEMENTS] = 
{
    [NO_LAYER_TYPE] = "NO_LAYER_TYPE", [INPUT_LAYER] = "INPUT_LAYER", [CONV_LAYER] = "CONV_LAYER", [FC_LAYER] = "FC_LAYER",
    [RESIDUAL_LAYER] = "RESIDUAL_LAYER", [BATCHNORM_LAYER] = "BATCHNORM_LAYER", [YOLO_LAYER] = "YOLO_LAYER", [APPEND_LAYER] = "APPEND_LAYER", [ACTIVATION_LAYER] = "ACTIVATION_LAYER", [MAXPOOL_LAYER] = "MAXPOOL_LAYER", [AVGPOOL_LAYER] = "AVGPOOL_LAYER",
    [ROUTE_LAYER] = "ROUTE_LAYER", [SOFTMAX_LAYER] = "SOFTMAX_LAYER",
    [MATMUL_LAYER] = "MATMUL_LAYER", [LAYERNORM_LAYER] = "LAYERNORM_LAYER", [K_ATTENTION_LAYER] = "K_ATTENTION_LAYER", [V_ATTENTION_LAYER] = "V_ATTENTION_LAYER"
};

char *param_type_str[NUM_PARAM_ELEMENTS] = 
{
    [OUT_W] = "OUT_W", [OUT_H] = "OUT_H", [IN_W] = "IN_W", [IN_H] = "IN_H", [IN_C] = "IN_C", [BATCH] = "BATCH", [OUT_C] = "OUT_C", [SUB_C] = "SUB_C", [WEIGHT_W] = "WEIGHT_W", [WEIGHT_H] = "WEIGHT_H", [STRIDE] = "STRIDE", [PADDING] = "PADDING", [DILATION] = "DILATION", [GROUPS] = "GROUPS",
    [NUM_HIDDEN] = "NUM_HIDDEN", [NUM_HEAD] = "NUM_HEAD", [NUM_SEQ] = "NUM_SEQ", [MAT_M] = "MAT_M", [MAT_N] = "MAT_N", [MAT_K] = "MAT_K", [SUB_M] = "SUB_M", [MASKED] = "MASKED",
    [FORM_BYTES] = "FORM_BYTES"
};

char *tensor_type_str[NUM_TENSORS] = 
{
    [NULL_TENSOR] = "NULL_TENSOR", [OUTPUT_TENSOR] = "OUTPUT_TENSOR", [INPUT_TENSOR] = "INPUT_TENSOR", [WEIGHT_TENSOR] = "WEIGHT_TENSOR", [BIAS_TENSOR] = "BIAS_TENSOR", [ANCHOR_TENSOR] = "ANCHOR_TENSOR", [BN_VAR_TENSOR] = "BN_VAR_TENSOR", [BN_MEAN_TENSOR] = "BN_MEAN_TENSOR", [BN_WEIGHT_TENSOR] = "BN_WEIGHT_TENSOR"
};

char *parent_type_str[NUM_PARENT_ELEMENTS] = 
{
    [PARENT_NONE] = "PARENT_NONE", [PARENT_0] = "PARENT_0", [PARENT_1] = "PARENT_1", [PARENT_WEIGHT] = "PARENT_WEIGHT",
};

char *activation_type_str [NUM_ACTIVATIONS] = 
{
    [NO_ACTIVATION] = "NO_ACTIVATION", [SIGMOID] = "SIGMOID", [LINEAR] = "LINEAR", [TANH] = "TANH", [RELU] = "RELU", [LEAKY_RELU] = "LEAKY_RELU", [ELU] = "ELU", [SELU] = "SELU", [GELU] = "GELU"
};

char *rpool_cond_str [NUM_RPOOL_CONDS] = 
{
    [RPOOL_DNN] = "RPOOL_DNN", [RPOOL_LAYER_TYPE] = "RPOOL_LAYER_TYPE", [RPOOL_LAYER_IDX] = "RPOOL_LAYER_IDX", [RPOOL_NASM] = "RPOOL_NASM", [RPOOL_DSE] = "RPOOL_DSE"
};

unsigned int dynamic_mem_init = 0;
void **dynamic_arr[DYNAMIC_ALLOC_RANGE] = {NULL};
size_t dynamic_arr_mem_size[DYNAMIC_ALLOC_RANGE] = {0};
unsigned int dynamic_arr_max_num[DYNAMIC_ALLOC_RANGE] = {0};
pthread_mutex_t dynamic_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

void init_dynamic_mem ()
{
    pthread_mutex_lock (&dynamic_mutex);
    if (dynamic_mem_init == 1)
    {
        pthread_mutex_unlock (&dynamic_mutex);
        return;
    }
    for (int i = 0; i < DYNAMIC_ALLOC_RANGE; i++)
    {
        dynamic_arr[i] = calloc (DYNAMIC_ALLOC_ARR_INIT_SIZE, sizeof(void*));
        dynamic_arr_max_num[i] = DYNAMIC_ALLOC_ARR_INIT_SIZE;
        dynamic_arr_mem_size[i] = (DYNAMIC_ALLOC_MIN_SIZE) * (pow (DYNAMIC_ALLOC_RANGE_SCALE, i));
    }
    dynamic_mem_init = 1;
    // print_dynamic_mem_arr();
    pthread_mutex_unlock (&dynamic_mutex);
}

void increase_dynamic_mem_arr (int idx)
{
    void **temp = calloc (dynamic_arr_max_num[idx] + DYNAMIC_ALLOC_ARR_INIT_SIZE, sizeof(void*));
    memcpy (temp, dynamic_arr[idx], dynamic_arr_max_num[idx]*sizeof(void*));
    free (dynamic_arr[idx]);
    dynamic_arr[idx] = temp;
    dynamic_arr_max_num[idx] += DYNAMIC_ALLOC_ARR_INIT_SIZE;
}

void print_dynamic_mem_arr()
{
    if (dynamic_mem_init == 0)
        init_dynamic_mem ();
    for (size_t i = 0; i < DYNAMIC_ALLOC_RANGE; i++)
    {
        PRTF ("\t[%ld KiB]\t\t", dynamic_arr_mem_size[i]/1024);
        int count = 0;
        for (int j = 0; j < dynamic_arr_max_num[i]; j++)
        {
            if (dynamic_arr[i][j] != NULL)
                count++;
        }
        PRTF ("%d/%d\n", count, dynamic_arr_max_num[i]);
    }
}

void *aspen_dynamic_calloc (size_t num, size_t size)
{
    void *ptr = aspen_dynamic_malloc (num, size);
    if (ptr == NULL)
        bzero (ptr, num * size);
    return ptr;
}

void *aspen_dynamic_malloc (size_t num, size_t size)
{
    if (dynamic_mem_init == 0)
        init_dynamic_mem ();
    if (num*size <= 0)
        return NULL;
    #ifdef DEBUG
    else if (num*size > dynamic_arr_mem_size[DYNAMIC_ALLOC_RANGE-1])
    {
        ERROR_PRTF ("Error: Requested memory size %ld KiB is too large.\n", num*size/1024);
        assert (0);
    }
    #endif
    void* ptr = NULL;
    size_t i = 0;
    for (; i < DYNAMIC_ALLOC_RANGE; i++)
    {
        if (dynamic_arr_mem_size[i] >= num*size)
            break;
    }
    pthread_mutex_lock (&dynamic_mutex);
    for (int j = 0; j < dynamic_arr_max_num[i]; j++)
    {
        if (dynamic_arr[i][j] != NULL)
        {
            ptr = dynamic_arr[i][j];
            dynamic_arr[i][j] = NULL;
            break;
        }
    }
    pthread_mutex_unlock (&dynamic_mutex);
    if (ptr == NULL)
    {
        ptr = aspen_malloc (1, dynamic_arr_mem_size[i]);
    }
    return ptr;
}

void aspen_dynamic_free (void *ptr, size_t num, size_t size)
{
    if (dynamic_mem_init == 0)
        init_dynamic_mem ();
    if (num*size <= 0)
        return;
    #ifdef DEBUG
    else if (num*size > dynamic_arr_mem_size[DYNAMIC_ALLOC_RANGE-1])
    {
        ERROR_PRTF ("Error: Requested memory size %ld KiB is too large.\n", num*size/1024);
        assert (0);
    }
    #endif
    size_t i = 0, j = 0;
    for (; i < DYNAMIC_ALLOC_RANGE; i++)
    {
        if (dynamic_arr_mem_size[i] >= num*size)
            break;
    }
    pthread_mutex_lock (&dynamic_mutex);
    for (; j < dynamic_arr_max_num[i]; j++)
    {
        if (dynamic_arr[i][j] == NULL)
        {
            dynamic_arr[i][j] = ptr;
            break;
        }
    }
    if (j == dynamic_arr_max_num[i])
    {
        increase_dynamic_mem_arr (i);
        dynamic_arr[i][j] = ptr;
    }
    // print_dynamic_mem_arr();
    pthread_mutex_unlock (&dynamic_mutex);
}

void aspen_flush_dynamic_memory ()
{
    if (dynamic_mem_init == 0)
        return;
    pthread_mutex_lock (&dynamic_mutex);
    for (int i = 0; i < DYNAMIC_ALLOC_RANGE; i++)
    {
        for (int j = 0; j < dynamic_arr_max_num[i]; j++)
        {
            if (dynamic_arr[i][j] != NULL)
            {
                aspen_free (dynamic_arr[i][j]);
                dynamic_arr[i][j] = NULL;
            }
        }
    }
    pthread_mutex_unlock (&dynamic_mutex);
}

void *aspen_calloc (size_t num, size_t size)
{
    if (num*size <= 0)
        return NULL;
    void* ptr = NULL;
    if (aspen_num_gpus <= 0)
        ptr = aligned_alloc (MEM_ALIGN, get_smallest_dividable(num * size, MEM_ALIGN));   
    else
    {
        #ifdef GPU
        cudaError_t cuda_err = cudaMallocHost (&ptr, get_smallest_dividable (num * size, MEM_ALIGN));
        if (ptr == NULL || check_CUDA(cuda_err) != cudaSuccess)
        {
            ERROR_PRTF ("Error: Failed to allocate Host memory.\n");
            assert (0);
        }
        #endif
    }
    bzero (ptr, get_smallest_dividable (num * size, MEM_ALIGN));
    return ptr;
}
void *aspen_malloc (size_t num, size_t size)
{
    if (num*size <= 0)
        return NULL;
    void* ptr = NULL;
    if (aspen_num_gpus <= 0)
        ptr = aligned_alloc (MEM_ALIGN, get_smallest_dividable(num * size, MEM_ALIGN));   
    else
    {
        #ifdef GPU
        cudaError_t cuda_err = cudaMallocHost (&ptr, get_smallest_dividable (num * size, MEM_ALIGN));
        if (ptr == NULL || check_CUDA(cuda_err) != cudaSuccess)
        {
            ERROR_PRTF ("Error: Failed to allocate Host memory.\n");
            assert (0);
        }
        #endif
    }
    return ptr;
}
void aspen_free (void *ptr)
{
    if (ptr == NULL)
        return;
    if (aspen_num_gpus <= 0)
        free (ptr);
    else
    {
        #ifdef GPU
        if (check_CUDA(cudaFreeHost(ptr)) != cudaSuccess)
        {
            ERROR_PRTF ("Error: Failed to free Host memory.\n");
            assert (0);
        }
        #endif
    }
}

void *aspen_gpu_calloc (size_t num, size_t size, int gpu_idx)
{
    if (num*size <= 0)
        return NULL;
    void* ptr = NULL;
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_idx)) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaMalloc(&ptr, get_smallest_dividable (num * size, MEM_ALIGN) )) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to allocate GPU memory.\n");
        assert (0);
    }
    if (check_CUDA(cudaMemset(ptr, 0, get_smallest_dividable (num * size, MEM_ALIGN) )) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to set GPU memory to zero.\n");
        assert (0);
    }
    #endif
    return ptr;
}
void aspen_gpu_memset (void *ptr, int val, size_t size, int gpu_idx)
{
    if (size <= 0)
        return;
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_idx)) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaMemset(ptr, val, get_smallest_dividable (size, MEM_ALIGN) )) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to set GPU memory.\n");
        assert (0);
    }
    #endif
}
void *aspen_gpu_malloc_minus_one (size_t num, size_t size, int gpu_idx)
{
    if (num*size <= 0)
        return NULL;
    void* ptr = NULL;
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_idx)) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaMalloc(&ptr, get_smallest_dividable (num * size, MEM_ALIGN) )) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to allocate GPU memory.\n");
        assert (0);
    }
    if (check_CUDA(cudaMemset(ptr, 0xff, get_smallest_dividable (num * size, MEM_ALIGN) )) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to set GPU memory to zero.\n");
        assert (0);
    }
    #endif
    return ptr;
}
void *aspen_gpu_malloc (size_t num, size_t size, int gpu_idx)
{
    if (num*size <= 0)
        return NULL;
    void* ptr = NULL;
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_idx)) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaMalloc(&ptr, get_smallest_dividable (num * size, MEM_ALIGN) )) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to allocate GPU memory.\n");
        assert (0);
    }
    #endif
    return ptr;
}
void aspen_gpu_free (void *ptr, int gpu_idx)
{
    if (ptr == NULL)
        return;
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_idx)) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaFree(ptr)) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to free GPU memory.\n");
        assert (0);
    }
    #endif
}

void aspen_host_to_gpu_memcpy (void *dst, void *src, size_t num, int gpu_idx)
{
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_idx)) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaMemcpy(dst, src, num, cudaMemcpyHostToDevice)) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to copy Host memory to GPU.\n");
        assert (0);
    }
    #endif
}
void aspen_gpu_to_host_memcpy (void *dst, void *src, size_t num, int gpu_idx)
{
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_idx)) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaMemcpy(dst, src, num, cudaMemcpyDeviceToHost)) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to copy GPU memory to Host.\n");
        assert (0);
    }
    #endif
}
void aspen_host_to_gpu_async_memcpy (void *dst, void *src, size_t num, int gpu_idx)
{
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_idx)) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaMemcpyAsync(dst, src, num, cudaMemcpyHostToDevice
        , aspen_CUDA_streams[gpu_idx][GPU_MEM_STREAM_HOST_TO_GPU])) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to copy Host to GPU memory.\n");
        assert (0);
    }
    if (check_CUDA(cudaStreamSynchronize(aspen_CUDA_streams[gpu_idx][GPU_MEM_STREAM_HOST_TO_GPU])) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to synchronize GPU stream.\n");
        assert (0);
    }
    #endif
}
void aspen_gpu_to_host_async_memcpy (void *dst, void *src, size_t num, int gpu_idx)
{
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_idx)) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaMemcpyAsync(dst, src, num, cudaMemcpyDeviceToHost
        , aspen_CUDA_streams[gpu_idx][GPU_MEM_STREAM_GPU_TO_HOST])) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to copy GPU to Host memory.\n");
        assert (0);
    }
    if (check_CUDA(cudaStreamSynchronize(aspen_CUDA_streams[gpu_idx][GPU_MEM_STREAM_GPU_TO_HOST])) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to synchronize GPU stream.\n");
        assert (0);
    }
    #endif
}

void aspen_sync_gpu (int gpu_idx)
{
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_idx)) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaDeviceSynchronize()) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to synchronize GPU.\n");
        assert (0);
    }
    #endif
}

void aspen_sync_gpu_stream (int gpu_idx, int stream_num)
{
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_idx)) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaStreamSynchronize(aspen_CUDA_streams[gpu_idx][stream_num])) != cudaSuccess)
    {
        ERROR_PRTF ("Error: Failed to synchronize GPU stream.\n");
        assert (0);
    }
    #endif
}

int aspen_get_next_stream (int gpu_idx)
{
    static int stream_num[MAX_NUM_GPUS];
    stream_num[gpu_idx] = (stream_num[gpu_idx] + 1) % 32;
    return stream_num[gpu_idx];
}

size_t get_smallest_dividable (size_t num, size_t divider)
{
    return (num/divider + (num%divider != 0))*divider;
}

void* load_arr (char *file_path, unsigned int size)
{
    void *input = calloc (size, 1);
    FILE *fptr = fopen(file_path, "rb");
    if (fptr != NULL)
    {
        size_t num = fread (input, sizeof(char), size, fptr);
        if (num < size)
        {
            ERROR_PRTF ("Error: Failed to read file %s - Size mismatch. File size: %ld, Req. size: %d. Exiting.\n", 
                file_path, num, size);
            free (input);
            fclose(fptr);
            assert(0);
            return NULL;
        }
        fclose(fptr);
        return input;
    }
    else
    {
        ERROR_PRTF ("Error: Failed to open file %s. Exiting.\n", file_path);
        free (input);
        return NULL;
    }
}

void save_arr (void *input, char *file_path, unsigned int size)
{   
    FILE *fptr = fopen(file_path, "wb");
    fwrite (input, sizeof(char), size, fptr);
    fclose (fptr);
}

void fold_batchnorm_float (float *bn_var, float *bn_mean, float *bn_weight, 
    float *weight, float *bias, int cout, int cin, int hfil, int wfil)
{
    const double epsilon = 1e-5;
    
    for (int i = 0; i < cout; i++)
    {
        for (int j = 0; j < cin*hfil*wfil; j++)
        {
            float weight_val = *(weight + i*cin*hfil*wfil + j);
            weight_val = weight_val*(*(bn_weight + i))/sqrtf(*(bn_var + i) + epsilon);
            *(weight + i*cin*hfil*wfil + j) = weight_val;
        }
        float bias_val = *(bias + i);
        bias_val = bias_val - *(bn_weight + i)*(*(bn_mean + i))/sqrtf(*(bn_var + i) + epsilon);
        *(bias + i) = bias_val;
    }
}

void NHWC_to_NCHW (void *input, void *output, unsigned int n, unsigned int c, unsigned int h, unsigned int w, unsigned int element_size)
{
    if (input == NULL)
    {
        PRTF ("Error: Input is NULL.\n");
        return;
    }
    if (output == NULL)
    {
        PRTF ("Error: Output is NULL.\n");
        return;
    }
    if (input == output)
    {
        PRTF ("Error: Input and output are the same.\n");
        return;
    }
    for (int ni = 0; ni < n; ni++)
    {
        for (int ci = 0; ci < c; ci++)
        {
            for (int hi = 0; hi < h; hi++)
            {
                for (int wi = 0; wi < w; wi++)
                {
                    char* input_ptr = (char*)input + (ni*h*w*c + hi*w*c + wi*c + ci)*element_size;
                    char* output_ptr = (char*)output + (ni*c*h*w + ci*h*w + hi*w + wi)*element_size;
                    memcpy (output_ptr, input_ptr, element_size);
                }
            }
        }
    }
}
void NCHW_to_NHWC (void *input, void *output, unsigned int n, unsigned int c, unsigned int h, unsigned int w, unsigned int element_size)
{
    if (input == NULL)
    {
        PRTF ("Error: Input is NULL.\n");
        return;
    }
    if (output == NULL)
    {
        PRTF ("Error: Output is NULL.\n");
        return;
    }
    if (input == output)
    {
        PRTF ("Error: Input and output are the same.\n");
        return;
    }
    for (int ni = 0; ni < n; ni++)
    {
        for (int ci = 0; ci < c; ci++)
        {
            for (int hi = 0; hi < h; hi++)
            {
                for (int wi = 0; wi < w; wi++)
                {
                    char* input_ptr = (char*)input + (ni*c*h*w + ci*h*w + hi*w + wi)*element_size;
                    char* output_ptr = (char*)output + (ni*h*w*c + hi*w*c + wi*c + ci)*element_size;
                    memcpy (output_ptr, input_ptr, element_size);
                }
            }
        }
    }
}

void set_float_tensor_val (float *output, unsigned int n, unsigned int c, unsigned int h, unsigned int w)
{
    if (output == NULL)
    {
        PRTF ("Error: Output is NULL.\n");
        return;
    }
    for (int ni = 0; ni < n; ni++)
    {
        for (int ci = 0; ci < c; ci++)
        {
            for (int hi = 0; hi < h; hi++)
            {
                for (int wi = 0; wi < w; wi++)
                {
                    float* output_ptr = output + (ni*h*w*c + hi*w*c + wi*c + ci);
                    *output_ptr = (float)(ni*h*w*c + hi*w*c + wi*c + ci);
                }
            }
        }
    }
}

int compare_float_array (float *input1, float* input2, int num_to_compare, float epsilon_ratio, float epsilon_abs, int skip_val)
{
    int num = 0;
    PRTF ("Compare_array_f32 running...\n");
    // #pragma omp parallel for
    for (int i = 0; i < num_to_compare; i++)
    {
        float delta = fabsf(*(input1 + i) - *(input2 + i));
        if ((delta / fabsf(*(input1 + i))) >= epsilon_ratio && delta >= epsilon_abs)
        {
            num++;
            if (num < skip_val)
            {
                PRTF ("\tCompare failed at index %d. Value1: %3.3e, Value2: %3.3e, Diff: %1.2e (%2.2e%%)\n"
                    , i, *(input1 + i), *(input2 + i), delta, delta*100.0/(*(input1 + i)<0? -*(input1 + i):*(input1 + i)));
            }
            else if (num == skip_val)
            {
                PRTF ("\tToo many errors... (More than %d)\n", skip_val);
            }
        }
    }
    PRTF ("Compare_array_f32 complete.\nTotal of %d errors detected out of %d SP floats, with epsilon ratio of %1.1e.\n", num, num_to_compare,epsilon_ratio);
    return num;
}

int compare_float_tensor (float *input1, float* input2, int n, int c, int h ,int w, float epsilon_ratio, float epsilon_abs, int skip_val)
{
    int num = 0;
    PRTF ("Compare_tensor_f32 running...\n");
    // #pragma omp parallel for
    for (int ni = 0; ni < n; ni++)
    {
        for (int ci = 0; ci < c; ci++)
        {
            for (int hi = 0; hi < h; hi++)
            {
                for (int wi = 0; wi < w; wi++)
                {
                    int i = ni*c*h*w + ci*h*w + hi*w + wi;
                    float delta = fabs(*(input1 + i) - *(input2 + i));
                    if ((delta / fabs(*(input1 + i))) >= epsilon_ratio && delta >= epsilon_abs)
                    {
                        num++;
                        if (num < skip_val)
                        {
                            PRTF ("\tCompare failed at index (%d, %d, %d, %d). Value1: %3.3e, Value2: %3.3e, Diff: %1.2e (%2.2e%%)\n"
                                , ni, ci, hi, wi, *(input1 + i), *(input2 + i), delta, delta*100.0/(*(input1 + i)<0? -*(input1 + i):*(input1 + i)));
                        }
                        else if (num == skip_val)
                        {
                            PRTF ("\tToo many errors... (More than %d)\n", skip_val);
                        }
                    }
                    // else
                    // {
                    //     PRTF ("\t\tCompare passed at index (%d, %d, %d, %d). Value1: %3.3e, Value2: %3.3e, Diff: %1.2e (%2.2e%%)\n"
                    //         , ni, ci, hi, wi, *(input1 + i), *(input2 + i), delta, delta*100.0/(*(input1 + i)<0? -*(input1 + i):*(input1 + i)));
                    // }
                }
            }
        }
    }
    PRTF ("Compare_tensor_f32 complete.\nTotal of %d errors detected out of %d SP floats, with epsilon ratio of %1.1e.\n", num, n*c*h*w, epsilon_ratio);
    return num;
}

unsigned int get_cpu_count()
{
    cpu_set_t cs;
    CPU_ZERO(&cs);
    sched_getaffinity(0, sizeof(cs), &cs);

    unsigned int count = 0;
    for (int i = 0; i < 256; i++)
    {
        if (CPU_ISSET(i, &cs))
            count++;
        else
            break;
    }
    return count;
}

void get_probability_results (char *class_data_path, float* probabilities, unsigned int num)
{
    int buffer_length = 256;
    char buffer[num][buffer_length];
    FILE *fptr = fopen(class_data_path, "r");
    if (fptr == NULL)
    {
        PRTF ("Error in get_probability_results: Cannot open file %s.\n", class_data_path);
        return;
    }
    for (int i = 0; i < num; i++)
    {
        void *tmp = fgets(buffer[i], buffer_length, fptr);
        if (tmp == NULL)
        {
            PRTF ("Error in get_probability_results: Cannot read file %s.\n", class_data_path);
            return;
        }
        for (char *ptr = buffer[i]; *ptr != '\0'; ptr++)
        {
            if (*ptr == '\n')
            {
                *ptr = '\0';
            }
        }
    }
    fclose(fptr);
    PRTF ("Results:\n");
    for (int i = 0; i < 5; i++)
    {
        float max_val = -INFINITY;
        int max_idx = 0;
        for (int j = 0; j < num; j++)
        {
            if (max_val < *(probabilities + j))
            {
                max_val = *(probabilities + j);
                max_idx = j;
            }
        }
        PRTF ("%d: %s - %2.2f%%\n", i+1, buffer[max_idx], max_val*100);
        *(probabilities + max_idx) = -INFINITY;
    }
}

double get_time_last = -1;
double time_offset[SCHEDULE_MAX_DEVICES] = {0.0, };

double get_time_secs ()
{
    static int initiallized = 0;
    static struct timeval zero_time;
    if (initiallized == 0)
    {
        initiallized = 1;
        gettimeofday (&zero_time, NULL);
    }
    struct timeval now;
    gettimeofday (&now, NULL);
    long sec = now.tv_sec - zero_time.tv_sec;
    long usec = now.tv_usec - zero_time.tv_usec;
    return sec + usec*1e-6;
}

double get_time_secs_offset (int edge_id)
{
    return get_time_secs() + time_offset[edge_id];
}

void set_time_offset(double offset, DEVICE_MODE device_mode, int edge_id)
{
    if(device_mode == DEV_SERVER)
    {
        time_offset[edge_id] = -offset;
    }
    // if(device_mode == DEV_EDGE)
    // {
    //     time_offset = offset;
    // }
}

void get_elapsed_time (char *name)
{
    static size_t call_num = 0;
    if (get_time_last < 0)
    {
        get_time_last = get_time_secs();
    }
    double now = get_time_secs();
    double elapsed = now - get_time_last;
    printf ("Time measurement %s (%ld): %6.6f - %6.6f secs elapsed since last measurement.\n", name, call_num, now, elapsed);
    call_num++;
    get_time_last = now;
}

void get_elapsed_time_only()
{
    if (get_time_last < 0)
    {
        get_time_last = get_time_secs();
    }
    double now = get_time_secs();
    double elapsed = now - get_time_last;
    printf ("%6.6f", elapsed);
    get_time_last = now;
}

double get_elapsed_time_only_return()
{
    if (get_time_last < 0)
    {
        get_time_last = get_time_secs();
    }
    double now = get_time_secs();
    double elapsed = now - get_time_last;
    get_time_last = now;
    return elapsed;
}

void set_elapsed_time_start()
{
    if (get_time_last < 0)
    {
        get_time_last = get_time_secs();
    }
    double now = get_time_secs();
    get_time_last = now;
}

double get_elapsed_time_start()
{
    if (get_time_last < 0)
    {
        get_time_last = get_time_secs();
    }
    double now = get_time_secs();
    get_time_last = now;
    return now;
}

void print_float_array (float *input, int num, int newline_num)
{
    int i;
    PRTF ("Printing Array of size %d...\n", num);
    for (i = 0; i < num; i++)
    {
        const float val = *(input + i);
        if (val < 0.0)
        {
            PRTF ("\t%3.3ef", val);
        }
        else
        {
            PRTF ("\t %3.3ef", val);
        }
        if (i%newline_num == newline_num-1)
        {
            PRTF("\n");
        }
    }
    if (i%newline_num != newline_num-1)
    {
        PRTF("\n");
    }
}

void print_float_tensor (float *input, int n, int c, int h, int w)
{
    PRTF ("\t");
    int size_arr[] = {n, c, h};
    int newline = w;
    for (int i = 2; i >= 0; i--)
    {
        newline *= size_arr[i];
        if (newline > 20)
        {
            newline /= size_arr[i];
            break;
        }
    }
    size_t idx = 0;
    for (int ni = 0; ni < n; ni++)
    {
        for (int ci = 0; ci < c; ci++)
        {
            for (int hi = 0; hi < h; hi++)
            {
                for (int wi = 0; wi < w; wi++)
                {
                    if (idx%newline == 0)
                    {
                        PRTF("\n(%d, %d, %d, %d):\t", ni, ci, hi, wi);
                    }
                    const float val = *(input + ni*c*h*w + ci*h*w + hi*w + wi);
                    if (val < 0.0)
                    {
                        PRTF ("\t%3.3ef", val);
                    }
                    else
                    {
                        PRTF ("\t %3.3ef", val);
                    }
                    idx++;
                    
                }
            }
        }
    }
    PRTF ("\n");
}

void save_ninst_log(FILE* log_fp, nasm_t* nasm)
{
    fprintf(log_fp,"idx,thread id,computed time (ms),received time (ms),sent time (ms), eft offloaded (ms),eft server (ms)\n");
    for(int i = 0; i < nasm->num_ninst; i++)
    {
        ninst_t* ninst = &nasm->ninst_arr[i];
        fprintf(log_fp, "%d,%d,%f,%f,%f,%f,%f,%f\n",ninst->ninst_idx, ninst->dse_idx, ninst->computed_time*1000.0, ninst->received_time*1000.0, ninst->sent_time*1000.0, ninst->eft_offloaded, ninst->eft_server);
    }
    fflush(log_fp);
}

ssize_t read_n(int fd, void *buf, size_t n) {
        size_t bytes_read = 0;
    while(bytes_read < n) {
        bytes_read += read(fd, buf + bytes_read, n - bytes_read);
    }

    return n;
}

ssize_t write_n(int fd, void *buf, size_t n) {
        size_t bytes_written = 0;
    while(bytes_written < n) {
        bytes_written += write(fd, buf + bytes_written, n - bytes_written);
    }

    return n;
}

void create_connection(DEVICE_MODE dev_mode, char *server_ip, int control_port, int *server_sock, int *client_sock) {
    char connection_key[64];

    if (dev_mode == DEV_SERVER) {
        strcpy(connection_key, "hello world!");

        *server_sock = create_server_sock(server_ip, control_port);
        PRTF ("\tcreated server sock: %d\n", *server_sock);
        *client_sock = accept_client_sock(*server_sock);
        PRTF ("\taccepted client sock: %d\n", *client_sock);
        write_n(*client_sock, connection_key, sizeof(char)*64);
        PRTF ("\twrote connection key: %s\n", connection_key);
    }
    else if (dev_mode == DEV_EDGE) {
        // CREATE CONNECTION TO SERVER
        *server_sock = connect_server_sock(server_ip, control_port);
        PRTF ("\tconnected server sock: %d\n", *server_sock);
        read_n(*server_sock, &connection_key, sizeof(char)*64);
        PRTF ("\tread connection key: %s\n", connection_key);
    }
}

int create_server_sock(char *server_ip, int server_port) {
    int server_sock;
    struct sockaddr_in server_addr;

    // open server
    server_sock = socket(PF_INET, SOCK_STREAM, 0);
    int option = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    if (server_sock == -1) {
        PRTF("Error: socket() returned -1\n");
        assert(0);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(server_port);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        PRTF("Error: bind() returned -1\n");
        assert(0);
    }

    if (listen(server_sock, SCHEDULE_MAX_DEVICES) == -1) {
        PRTF("Error: listen() returned -1\n");
        assert(0);
    }

    return server_sock;
}

int accept_client_sock(int server_sock) {
    int client_sock;
    struct sockaddr_in client_addr;
    
    socklen_t client_addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_size);
    if (client_sock == -1) {
        PRTF("Error: accept() returned -1\n");
        assert(0);
    }

    return client_sock;
}

int connect_server_sock(char *server_ip, int server_port) {
    int server_sock;
    struct sockaddr_in server_addr;

    // connect to server
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        PRTF("Error: socket() returned -1\n");
        assert(0);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(server_port);

    size_t num_tries = 0;
    while (connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) 
    {
        PRTF("Connecting...\n");
        if (num_tries > 120000) 
        {
            PRTF("Error: connect() returned -1\n");
            assert(0);
        }
        num_tries++;
        usleep(5000);
    }

    return server_sock;
}

float get_max_recv_time(nasm_t* nasm)
{
    float max_recv_time = 0;
    for(int i = 0; i < nasm->num_ninst; i++)
    {
        if(nasm->ninst_arr[i].received_time != 0)
        {
            if(nasm->ninst_arr[i].received_time > max_recv_time)
                max_recv_time = nasm->ninst_arr[i].received_time;
        }
    }
    return max_recv_time;
}

float get_min_recv_time(nasm_t* nasm)
{
    float min_recv_time = 100000000;
    for(int i = 0; i < nasm->num_ninst; i++)
    {
        if(nasm->ninst_arr[i].received_time != 0)
        {
            if(nasm->ninst_arr[i].received_time < min_recv_time)
                min_recv_time = nasm->ninst_arr[i].received_time;
        }
    }
    return min_recv_time;
}

float get_max_sent_time(nasm_t* nasm)
{
    float max_sent_time = 0;
    for(int i = 0; i < nasm->num_ninst; i++)
    {
        if(nasm->ninst_arr[i].sent_time != 0)
        {
            if(nasm->ninst_arr[i].sent_time > max_sent_time)
                max_sent_time = nasm->ninst_arr[i].sent_time;
        }
    }
    return max_sent_time;
}

float get_min_sent_time(nasm_t* nasm)
{
    float min_sent_time = 100000000;
    for(int i = 0; i < nasm->num_ninst; i++)
    {
        if(nasm->ninst_arr[i].sent_time != 0)
        {
            if(nasm->ninst_arr[i].sent_time < min_sent_time)
                min_sent_time = nasm->ninst_arr[i].sent_time;
        }
    }
    return min_sent_time;
}

float get_max_computed_time(nasm_t* nasm)
{
    float max_computed_time = 0;
    
    for(int i = 0; i < nasm->num_ninst; i++)
    {
        if(nasm->ninst_arr[i].computed_time != 0)
        {
            if(nasm->ninst_arr[i].computed_time > max_computed_time)
                max_computed_time = nasm->ninst_arr[i].computed_time;
        }
    }
    return max_computed_time;
}

float get_min_computed_time(nasm_t* nasm)
{
    float min_computed_time = 100000000;

    for(int i = 0; i < nasm->num_ninst; i++)
    {
        if(nasm->ninst_arr[i].computed_time != 0)
        {   
            if(nasm->ninst_arr[i].computed_time < min_computed_time)
                min_computed_time = nasm->ninst_arr[i].computed_time;
        }
    }
    return min_computed_time;
}

double get_sec()
{
    struct timeval now;
    gettimeofday (&now, NULL);
    return now.tv_sec + now.tv_usec*1e-6;
}

void softmax (float *input, float *output, unsigned int num_batch, unsigned int num_elements)
{
    for (int i = 0; i < num_batch; i++)
    {
        float max = input[i * num_elements];
        for (int j = 1; j < num_elements; j++)
        {
            if (input[i * num_elements + j] > max)
                max = input[i * num_elements + j];
        }
        float sum = 0;
        for (int j = 0; j < num_elements; j++)
        {
            output[i * num_elements + j] = expf (input[i * num_elements + j] - max);
            sum += output[i * num_elements + j];
        }
        for (int j = 0; j < num_elements; j++)
            output[i * num_elements + j] /= sum;
    }
}

void get_prob_results (char *class_data_path, float* probabilities, unsigned int num)
{
    int buffer_length = 256;
    char buffer[num][buffer_length];
    FILE *fptr = fopen(class_data_path, "r");
    if (fptr == NULL)
        assert (0);
    for (int i = 0; i < num; i++)
    {
        void *tmp = fgets(buffer[i], buffer_length, fptr);
        if (tmp == NULL)
            assert (0);
        for (char *ptr = buffer[i]; *ptr != '\0'; ptr++)
        {
            if (*ptr == '\n')
            {
                *ptr = '\0';
            }
        }
    }
    fclose(fptr);
    printf ("Results:\n");
    for (int i = 0; i < 5; i++)
    {
        float max_val = -INFINITY;
        int max_idx = 0;
        for (int j = 0; j < num; j++)
        {
            if (max_val < *(probabilities + j))
            {
                max_val = *(probabilities + j);
                max_idx = j;
            }
        }
        printf ("%d: %s - %2.2f%%\n", i+1, buffer[max_idx], max_val*100);
        *(probabilities + max_idx) = -INFINITY;
    }
}

int get_ldata_intsum (nasm_ldata_t *ldata) {
    int sum = 0;

    size_t size = ldata->out_mat_mem_size;
    int *buffer = (int *)calloc (size, 1);

    copy_ldata_out_mat_to_buffer(ldata, buffer);

    for (int i = 0; i < size/sizeof(int); i++) {
        sum += buffer[i];
    }

    free(buffer);

    return sum;

}

void print_progress_bar(char *prefix, int total, int now) {
    const char bar = '=';
	const char blank = ' ';
	const int len = 20;
    const int progress = len * now / total;
	
	float percent = (float)now / total * 100;
	
    printf("\r%s %d/%d [", prefix, now, total);
    
    for (int i=0; i<len; i++) {
        if(progress > i) {
            printf("%c", bar);
        } else {
            printf("%c", blank);
        }
    }
    printf("] %0.2f%%", percent);
    fflush(stdout);

    if (total == now) printf("\n");

}