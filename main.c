#include <stdio.h>
#include "aspen.h"
#include "util.h"
#include "nasm.h"
#include "apu.h"


int main(void)
{
    print_aspen_build_info();
    // aspen_dnn_t *resnet50_dnn = apu_create_dnn("data/cfg/resnet50.cfg", NULL);
    // if (resnet50_dnn == NULL) 
    // {
    //     printf("Error: Failed to create DNN\n");
    //     return -1;
    // }
    // print_dnn_info(resnet50_dnn, 0);
    // apu_save_dnn_to_file (resnet50_dnn, "data/resnet50.aspen");
    // aspen_dnn_t *resnet50_dnn_2 = apu_load_dnn_from_file ("data/resnet50.aspen");
    // if (resnet50_dnn_2 == NULL) 
    // {
    //     printf("Error: Failed to read DNN\n");
    //     return -1;
    // }
    // print_dnn_info (resnet50_dnn_2, 0);
    // nasm_t *resnet50_nasm = apu_create_nasm(resnet50_dnn, 5e6, 1);
    // if (resnet50_nasm == NULL) 
    // {
    //     printf("Error: Failed to create NASM\n");
    //     return -1;
    // }
    // print_nasm_info(resnet50_nasm, 0);
    // apu_save_nasm_to_file (resnet50_nasm, "data/resnet50.nasm");

    aspen_dnn_t *resnet50_dnn = NULL;
    nasm_t *resnet50_nasm = apu_load_nasm_from_file ("data/resnet50.nasm", &resnet50_dnn);
    // nasm_t *resnet50_4_nasm = apu_load_nasm_from_file ("data/resnet50_4.nasm", &resnet50_dnn);

    apu_load_dnn_data_from_file (resnet50_dnn, "data/resnet50_data.bin");
    unsigned int input_params[NUM_PARAM_ELEMENTS] =
        {[BATCH] = 4, [OUT_C] = 3, [OUT_H] = 224, [OUT_W] = 224};
    void *dog_data = aspen_load_input_from_file ("data/batched_input_64.bin", input_params, sizeof(float));
    // rpool_t *rpool = rpool_init (0);
    // ase_group_t *ase_group = ase_group_init (4, 0);
    // ase_group_set_rpool (ase_group, rpool);

    // rpool_add_nasm (rpool, resnet50_4_nasm, 0.5);
    // rpool_add_nasm (rpool, resnet50_nasm, 1.0);
    // // print_rpool_info (rpool);
    // // print_nasm_info(resnet50_4_nasm, 0);
    // print_dnn_info(resnet50_dnn, 0);

    // ase_group_run (ase_group);
    // ase_wait_for_nasm_completion (resnet50_nasm);
    // ase_wait_for_nasm_completion (resnet50_4_nasm);
    // ase_group_stop (ase_group);
    
    // print_nasm_info(resnet50_4_nasm, 0);
    // print_rpool_info (rpool);

    aspen_run_naive (resnet50_dnn, input_params[BATCH], dog_data);
    
    for (int i = 70; i < 74; i++)
    {
        aspen_layer_t *layer = &resnet50_dnn->layers[i];
        LAYER_PARAMS output_order[] = {BATCH, OUT_C, OUT_H, OUT_W};
        void *layer_output = get_aspen_tensor_data 
            (layer->tensors[OUTPUT_TENSOR], output_order);
        char filename[256];
        sprintf (filename, "resnet50_layer%d.bin", i);
        size_t data_size = layer->tensors[OUTPUT_TENSOR]->num_elements * sizeof(float);
        void *expected_output = load_arr (filename, layer->tensors[OUTPUT_TENSOR]->num_elements*sizeof(float));
        compare_float_tensor (layer_output, expected_output, 
            layer->params[BATCH], layer->params[OUT_C], layer->params[OUT_H], layer->params[OUT_W],
            layer->tensors[OUTPUT_TENSOR]->num_elements, 1e-4, 100);
        
        // printf ("Computed output for layer %d:\n", i);
        // print_float_tensor (dog_data, layer->params[BATCH], 
        //     layer->params[OUT_H], layer->params[OUT_W], layer->params[OUT_C]);
        // printf ("Expected output for layer %d:\n", i);
        // print_float_tensor (expected_output, layer->params[BATCH], layer->params[OUT_C], 
        //     layer->params[OUT_H], layer->params[OUT_W]);
        
        free (expected_output);
        free (layer_output);
    }

    aspen_layer_t *layer = &resnet50_dnn->layers[73];
    LAYER_PARAMS output_order[] = {BATCH, OUT_C, OUT_H, OUT_W};
    float *layer_output = get_aspen_tensor_data 
        (layer->tensors[OUTPUT_TENSOR], output_order);
    // print_float_array (layer_output, 1000*input_params[BATCH], 1000);
    for (int i = 0; i < input_params[BATCH]; i++)
    {
        get_probability_results ("data/imagenet_classes.txt", layer_output + 1000*i, 1000);
    }
    free (layer_output);

    // ase_group_destroy (ase_group);
    // rpool_destroy (rpool);
    // apu_destroy_nasm (resnet50_4_nasm);
    apu_destroy_nasm (resnet50_nasm);
    apu_destroy_dnn (resnet50_dnn);
    return 0;
}