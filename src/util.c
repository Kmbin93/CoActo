#include "util.h"

char *ninst_state_str[NUM_NINST_STATES] = 
{
    [NINST_NOT_READY] = "NINST_NOT_READY", [NINST_READY] = "NINST_READY", [NINST_COMPLETED] = "NINST_COMPLETED"
};

char *layer_type_str [NUM_LAYER_ELEMENTS] = 
{
    [NO_LAYER_TYPE] = "NO_LAYER_TYPE", [INPUT_LAYER] = "INPUT_LAYER", [CONV_LAYER] = "CONV_LAYER", [FC_LAYER] = "FC_LAYER",
    [RESIDUAL_LAYER] = "RESIDUAL_LAYER", [BATCHNORM_LAYER] = "BATCHNORM_LAYER", [YOLO_LAYER] = "YOLO_LAYER", [ACTIVATION_LAYER] = "ACTIVATION_LAYER", [MAXPOOL_LAYER] = "MAXPOOL_LAYER", [AVGPOOL_LAYER] = "AVGPOOL_LAYER",
    [ROUTE_LAYER] = "ROUTE_LAYER", [SOFTMAX_LAYER] = "SOFTMAX_LAYER"
};

char *param_type_str[NUM_PARAM_ELEMENTS] = 
{
    [OUT_W] = "OUT_W", [OUT_H] = "OUT_H", [IN_W] = "IN_W", [IN_H] = "IN_H", [IN_C] = "IN_C", [OUT_C] = "OUT_C", [WEIGHT_W] = "WEIGHT_W", [WEIGHT_H] = "WEIGHT_H", [STRIDE] = "STRIDE", [PADDING] = "PADDING", [DILATION] = "DILATION", [GROUPS] = "GROUPS",
    [SEQ_LEN] = "SEQ_LEN", [HEAD_NUM] = "HEAD_NUM", [HIDDEN_PER_HEAD] = "HIDDEN_PER_HEAD",
    [FORM_BYTES] = "FORM_BYTES"
};

char *tensor_type_str[NUM_TENSORS] = 
{
    [NULL_TENSOR] = "NULL_TENSOR", [OUTPUT_TENSOR] = "OUTPUT_TENSOR", [INPUT_TENSOR] = "INPUT_TENSOR", [WEIGHT_TENSOR] = "WEIGHT_TENSOR", [BIAS_TENSOR] = "BIAS_TENSOR", [BN_VAR_TENSOR] = "BN_VAR_TENSOR", [BN_MEAN_TENSOR] = "BN_MEAN_TENSOR", [BN_WEIGHT_TENSOR] = "BN_WEIGHT_TENSOR"
};

char *parent_type_str[NUM_PARENT_ELEMENTS] = 
{
    [PARENT_NONE] = "PARENT_NONE", [PARENT_0] = "PARENT_0", [PARENT_1] = "PARENT_1", [PARENT_WEIGHT] = "PARENT_WEIGHT",
};

char *activation_type_str [NUM_ACTIVATIONS] = 
{
    [NO_ACTIVATION] = "NO_ACTIVATION", [SIGMOID] = "SIGMOID", [LINEAR] = "LINEAR", [TANH] = "TANH", [RELU] = "RELU", [LEAKY_RELU] = "LEAKY_RELU", [ELU] = "ELU", [SELU] = "SELU"
};

char *rpool_cond_str [NUM_RPOOL_CONDS] = 
{
    [RPOOL_DNN] = "RPOOL_DNN", [RPOOL_LAYER_TYPE] = "RPOOL_LAYER_TYPE", [RPOOL_LAYER_IDX] = "RPOOL_LAYER_IDX", [RPOOL_NASM] = "RPOOL_NASM", [RPOOL_ASE] = "RPOOL_ASE"
};


void *aspen_calloc (size_t num, size_t size)
{
    if (num*size <= 0)
        return NULL;
    void* ptr = NULL;
    if (aspen_num_gpus < 0)
        ptr = aligned_alloc (MEM_ALIGN, get_smallest_dividable(num * size, MEM_ALIGN));   
    else
    {
        #ifdef GPU
        cudaError_t cuda_err = cudaMallocHost (&ptr, get_smallest_dividable (num * size, MEM_ALIGN));
        if (ptr == NULL || check_CUDA(cuda_err) != cudaSuccess)
        {
            FPRT (stderr, "Error: Failed to allocate Host memory.\n");
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
    if (aspen_num_gpus < 0)
        ptr = aligned_alloc (MEM_ALIGN, get_smallest_dividable(num * size, MEM_ALIGN));   
    else
    {
        #ifdef GPU
        cudaError_t cuda_err = cudaMallocHost (&ptr, get_smallest_dividable (num * size, MEM_ALIGN));
        if (ptr == NULL || check_CUDA(cuda_err) != cudaSuccess)
        {
            FPRT (stderr, "Error: Failed to allocate Host memory.\n");
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
    if (aspen_num_gpus < 0)
        free (ptr);
    else
    {
         #ifdef GPU
        if (check_CUDA(cudaFreeHost(ptr)) != cudaSuccess)
        {
            FPRT (stderr, "Error: Failed to free Host memory.\n");
            assert (0);
        }
        #endif
    }
}
void *aspen_gpu_calloc (size_t num, size_t size, int gpu_num)
{
    if (num*size <= 0)
        return NULL;
    void* ptr = NULL;
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_num)) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaMalloc(&ptr, get_smallest_dividable (num * size, MEM_ALIGN) )) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to allocate GPU memory.\n");
        assert (0);
    }
    if (check_CUDA(cudaMemset(ptr, 0, get_smallest_dividable (num * size, MEM_ALIGN) )) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to set GPU memory to zero.\n");
        assert (0);
    }
    #endif
    return ptr;
}
void *aspen_gpu_malloc (size_t num, size_t size, int gpu_num)
{
    if (num*size <= 0)
        return NULL;
    void* ptr = NULL;
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_num)) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaMalloc(&ptr, get_smallest_dividable (num * size, MEM_ALIGN) )) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to allocate GPU memory.\n");
        assert (0);
    }
    #endif
    return ptr;
}
void aspen_gpu_free (void *ptr, int gpu_num)
{
    if (ptr == NULL)
        return;
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_num)) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaFree(ptr)) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to free GPU memory.\n");
        assert (0);
    }
    #endif
}

void aspen_host_to_gpu_memcpy (void *dst, void *src, size_t num, int gpu_num)
{
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_num)) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaMemcpy(dst, src, num, cudaMemcpyHostToDevice)) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to copy Host to GPU memory.\n");
        assert (0);
    }
    #endif
}
void aspen_gpu_to_host_memcpy (void *dst, void *src, size_t num, int gpu_num)
{
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_num)) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaMemcpy(dst, src, num, cudaMemcpyDeviceToHost)) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to copy GPU to Host memory.\n");
        assert (0);
    }
    #endif
}
void aspen_host_to_gpu_async_memcpy (void *dst, void *src, size_t num, int gpu_num)
{
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_num)) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaMemcpyAsync(dst, src, num, cudaMemcpyHostToDevice
        , aspen_CUDA_streams[gpu_num][GPU_MEM_STREAM_HOST_TO_GPU])) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to copy Host to GPU memory.\n");
        assert (0);
    }
    if (check_CUDA(cudaStreamSynchronize(aspen_CUDA_streams[gpu_num][GPU_MEM_STREAM_HOST_TO_GPU])) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to synchronize GPU stream.\n");
        assert (0);
    }
    #endif
}
void aspen_gpu_to_host_async_memcpy (void *dst, void *src, size_t num, int gpu_num)
{
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_num)) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaMemcpyAsync(dst, src, num, cudaMemcpyDeviceToHost
        , aspen_CUDA_streams[gpu_num][GPU_MEM_STREAM_GPU_TO_HOST])) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to copy GPU to Host memory.\n");
        assert (0);
    }
    if (check_CUDA(cudaStreamSynchronize(aspen_CUDA_streams[gpu_num][GPU_MEM_STREAM_GPU_TO_HOST])) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to synchronize GPU stream.\n");
        assert (0);
    }
    #endif
}

void aspen_sync_gpu (int gpu_num)
{
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_num)) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaDeviceSynchronize()) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to synchronize GPU.\n");
        assert (0);
    }
    #endif
}

void aspen_sync_gpu_stream (int gpu_num, int stream_num)
{
    #ifdef GPU
    if (check_CUDA(cudaSetDevice(gpu_num)) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to set GPU device.\n");
        assert (0);
    }
    if (check_CUDA(cudaStreamSynchronize(aspen_CUDA_streams[gpu_num][stream_num])) != cudaSuccess)
    {
        FPRT (stderr, "Error: Failed to synchronize GPU stream.\n");
        assert (0);
    }
    #endif
}

int aspen_get_next_stream (int gpu_num)
{
    static int stream_num[MAX_NUM_GPUS];
    stream_num[gpu_num] = (stream_num[gpu_num] + 1) % 32;
    return stream_num[gpu_num];
}

unsigned int get_smallest_dividable (unsigned int num, unsigned int divider)
{
    return (num/divider + (num%divider != 0))*divider;
}

void* load_arr (char *file_path, unsigned int size)
{
    void *input = calloc (size, 1);
    FILE *fptr = fopen(file_path, "rb");
    if (fptr != NULL)
    {
        fread (input, sizeof(char), size, fptr);
        fclose(fptr);
        return input;
    }
    else
    {
        FPRT (stderr, "Error: Failed to open file %s. Exiting.\n", file_path);
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
        printf ("Error: Input is NULL.\n");
        return;
    }
    if (output == NULL)
    {
        printf ("Error: Output is NULL.\n");
        return;
    }
    if (input == output)
    {
        printf ("Error: Input and output are the same.\n");
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
        printf ("Error: Input is NULL.\n");
        return;
    }
    if (output == NULL)
    {
        printf ("Error: Output is NULL.\n");
        return;
    }
    if (input == output)
    {
        printf ("Error: Input and output are the same.\n");
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
        printf ("Error: Output is NULL.\n");
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

int compare_float_array (float *input1, float* input2, int num_to_compare, float epsilon_ratio, int skip_val)
{
    int num = 0;
    printf ("Compare_array_f32 running...\n");
    // #pragma omp parallel for
    for (int i = 0; i < num_to_compare; i++)
    {
        float delta = fabsf(*(input1 + i) - *(input2 + i));
        if ((delta / fabsf(*(input1 + i))) >= epsilon_ratio)
        {
            num++;
            if (num < skip_val)
            {
                printf ("\tCompare failed at index %d. Value1: %3.3e, Value2: %3.3e, Diff: %1.2e (%2.2e%%)\n"
                    , i, *(input1 + i), *(input2 + i), delta, delta*100.0/(*(input1 + i)<0? -*(input1 + i):*(input1 + i)));
            }
            else if (num == skip_val)
            {
                printf ("\tToo many errors... (More than %d)\n", skip_val);
            }
        }
    }
    printf ("Compare_array_f32 complete.\nTotal of %d errors detected out of %d SP floats, with epsilon ratio of %1.1e.\n", num, num_to_compare,epsilon_ratio);
    return num;
}

void get_probability_results (char *class_data_path, float* probabilities, unsigned int num)
{
    int buffer_length = 256;
    char buffer[num][buffer_length];
    FILE *fptr = fopen(class_data_path, "r");
    if (fptr == NULL)
    {
        printf ("Error in get_probability_results: Cannot open file %s.\n", class_data_path);
        return;
    }
    for (int i = 0; i < num; i++)
    {
        fgets(buffer[i], buffer_length, fptr);
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


int compare_float_tensor (float *input1, float* input2, int n, int c, int h ,int w, int num_to_compare, float epsilon_ratio, int skip_val)
{
    int num = 0;
    printf ("Compare_tensor_f32 running...\n");
    // // #pragma omp parallel for
    for (int ni = 0; ni < n; ni++)
    {
        for (int ci = 0; ci < c; ci++)
        {
            for (int hi = 0; hi < h; hi++)
            {
                for (int wi = 0; wi < w; wi++)
                {
                    int i = ni*c*h*w + ci*h*w + hi*w + wi;
                    float delta = *(input1 + i) - *(input2 + i);
                    if (delta < 0.0)
                    {
                        delta = 0 - delta;
                    }
                    if ((delta / *(input1 + i)) >= epsilon_ratio)
                    {
                        num++;
                        if (num < skip_val)
                        {
                            printf ("\tCompare failed at index (%d, %d, %d, %d). Value1: %3.3e, Value2: %3.3e, Diff: %1.2e (%2.2e%%)\n"
                                , ni, ci, hi, wi, *(input1 + i), *(input2 + i), delta, delta*100.0/(*(input1 + i)<0? -*(input1 + i):*(input1 + i)));
                        }
                        else if (num == skip_val)
                        {
                            printf ("\tToo many errors... (More than %d)\n", skip_val);
                        }
                    }
                }
            }
        }
    }
    printf ("Compare_tensor_f32 complete.\nTotal of %d errors detected out of %d SP floats, with epsilon ratio of %1.1e.\n", num, n*c*h*w, epsilon_ratio);
    return num;
}

void print_float_array (float *input, int num, int newline_num)
{
    int i;
    printf ("Printing Array of size %d...\n", num);
    for (i = 0; i < num; i++)
    {
        const float val = *(input + i);
        if (val < 0.0)
        {
            printf ("\t%3.3ef", val);
        }
        else
        {
            printf ("\t %3.3ef", val);
        }
        if (i%newline_num == newline_num-1)
        {
            printf("\n");
        }
    }
    if (i%newline_num != newline_num-1)
    {
        printf("\n");
    }
}

void print_float_tensor (float *input, int n, int c, int h, int w)
{
    printf ("\t");
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
                        printf("\n(%d, %d, %d, %d):\t", ni, ci, hi, wi);
                    }
                    const float val = *(input + ni*c*h*w + ci*h*w + hi*w + wi);
                    if (val < 0.0)
                    {
                        printf ("\t%3.3ef", val);
                    }
                    else
                    {
                        printf ("\t %3.3ef", val);
                    }
                    idx++;
                    
                }
            }
        }
    }
    printf ("\n");
}