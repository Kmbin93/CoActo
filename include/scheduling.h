#ifndef _SCHEDULING_H_
#define _SCHEDULING_H_

#define SCHEDULE_INIT_BUF_SIZE      (1024 * 1024)
#define PROFILE_REPEAT              4
#define PROFILE_LONG_MESSAGE_SIZE   (1024 * 256)
#define SCHEDULE_MAX_DEVICES        9

#define SCHEDULE_MAX_CORE           64

#include "nasm.h"
#include "profiling.h"
#include "aspen.h"

#include <float.h>
#include <limits.h>
#include <stdatomic.h>


typedef enum {
    FULL_OFFLOAD,
    PARTIAL_OFFLOAD,
    HEFT,
    CPOP
} schedule_policy_t;

typedef enum {
    SYNCHRONIZE,
    HEFT_COMPUTATION_COST,
    HEFT_TRANSMIT_RATE,
    HEFT_SYNC
} schedule_meta_type_t;

struct sched_task_t {
    int idx;
    sched_processor_t *processor;
    float start_time;
    float end_time;
    sched_task_t *next;
    sched_task_t *prev;
};

struct sched_processor_t {
    int idx;
    int num_task;
    sched_task_t *task_list;
};

struct dynamic_scheduler_t{
    float avg_server_ninst_compute_time[SCHEDULE_MAX_DEVICES];
    float avg_edge_ninst_compute_time[SCHEDULE_MAX_DEVICES];
    float avg_bandwidth[SCHEDULE_MAX_DEVICES];
    float rtt[SCHEDULE_MAX_DEVICES];

    int edge_num_dse[SCHEDULE_MAX_DEVICES];
    int server_num_dse[SCHEDULE_MAX_DEVICES];

    float scheduling_latency[SCHEDULE_MAX_DEVICES]; // obtained by scheduling policy in cloud
};

struct spinn_scheduler_t{
    // split candidates of SPINNN are ReLU layers
    int *split_candidates[SCHEDULE_MAX_DEVICES];
    int num_split_candidates[SCHEDULE_MAX_DEVICES];

    int current_split_layer[SCHEDULE_MAX_DEVICES];

    // For network profile
    float avg_bandwidth[SCHEDULE_MAX_DEVICES];
    float rtt[SCHEDULE_MAX_DEVICES];
    int *data_size_split_candidates[SCHEDULE_MAX_DEVICES];

    // For computation profile
    float avg_server_ninst_compute_time[SCHEDULE_MAX_DEVICES];
    float avg_edge_ninst_compute_time[SCHEDULE_MAX_DEVICES];
    int edge_num_dse[SCHEDULE_MAX_DEVICES];
    int server_num_dse[SCHEDULE_MAX_DEVICES];
    
    // For layer profile
    float *server_offline_layer_latency[SCHEDULE_MAX_DEVICES]; //[SCHEDULE_MAX_DEVICES][MAX_LAYERS];
    float *server_real_latency[SCHEDULE_MAX_DEVICES];       //[SCHEDULE_MAX_DEVICES][MAX_LAYERS];
    float *edge_offline_layer_latency[SCHEDULE_MAX_DEVICES]; //[SCHEDULE_MAX_DEVICES][MAX_LAYERS];
    float *edge_real_latency[SCHEDULE_MAX_DEVICES];       //[SCHEDULE_MAX_DEVICES][MAX_LAYERS];
    
    // SF = T_real / T_offline for all split candidates
    float edge_scaling_factors[SCHEDULE_MAX_DEVICES]; //[SCHEDULE_MAX_DEVICES];
    float server_scaling_factors[SCHEDULE_MAX_DEVICES]; //[SCHEDULE_MAX_DEVICES];

};

struct fl_path_layer_t {
    fl_path_t *fl_path;

    nasm_ldata_t *ldata;

    ninst_t **ninst_ptr_arr;
    unsigned int num_ninsts;
    atomic_uint num_ninsts_completed;

};

struct fl_path_t {
    unsigned int path_idx;

    unsigned int num_path_layers;
    atomic_uint num_path_layers_completed;
    fl_path_layer_t *path_layers_arr;

    unsigned int edge_final_layer_idx;
};

int is_offloaded(ninst_t *ninst);
int is_dev_compute(ninst_t *ninst, int device_idx);
int is_core_compute(ninst_t *ninst, int core_idx);
void ninst_clear_compute_device(ninst_t *ninst);
void ninst_set_compute_device(ninst_t *ninst, int device_idx);
void ninst_set_send_target_device(ninst_t *ninst, int device_idx);
void ninst_clear_send_target_device(ninst_t *ninst);
void ninst_copy_compute_device(ninst_t* target_ninst, ninst_t* ninst);

void ninst_core_allow_all(ninst_t *ninst);
void ninst_core_disallow_all(ninst_t *ninst);
void ninst_core_allow(ninst_t *ninst, int core_idx, int allow);
void ninst_core_allow_rand(ninst_t *ninst, int num_core);
int get_allowed_core_idx(ninst_t *ninst);

void core_init_random(nasm_t *nasm, int num_core);

dynamic_scheduler_t* init_dynamic_scheduler(avg_ninst_profile_t **ninst_profile, network_profile_t **network_profile, DEVICE_MODE device_mode, int device_idx, int num_edge_devices);
spinn_scheduler_t* init_spinn_scheduler(avg_ninst_profile_t **ninst_profile, network_profile_t **network_profile, nasm_t** nasms, DEVICE_MODE device_mode, int device_idx, int num_edge_devices);

void spinn_update_profile(spinn_scheduler_t* spinn_scheduler, float rtt, float avg_bandwidth, float avg_edge_latency, float avg_server_latency, int device_idx);
void spinn_model_splitter(spinn_scheduler_t* spinn_scheduler, nasm_t* nasm, int device_idx);
int spinn_schedule_layer(spinn_scheduler_t* spinn_scheduler, nasm_t* nasm, int device_idx);

void init_allow_all(nasm_t *nasm, int num_dev);
void init_full_local(nasm_t *nasm, int dev_idx);
void init_full_offload(nasm_t *nasm, int edge_id, int server_id);
void init_partial_offload(nasm_t *nasm, int split_layer, float compute_ratio, int edge_id, int server_id);
void init_random_offload(nasm_t *nasm, float compute_ratio, int edge_id, int server_id);
void init_sequential_offload(nasm_t *nasm, int split_layer, int edge_id, int server_id);
void init_dynamic_offload(nasm_t *nasm, DEVICE_MODE device_mode, int edge_id, int server_id);
void init_conventional_offload(nasm_t *nasm, int edge_id, int server_id);
sched_processor_t *init_heft(char *target_dnn_dir, char *target_nasm_dir, ninst_profile_t **ninst_profile, network_profile_t *network_profile, int num_device);

void heft_gen_dependency(nasm_t *nasm, int **dependency);
void heft_gen_data(nasm_t *nasm, ninst_profile_t **ninst_profile, int **dependency, float **data);
void heft_gen_W(nasm_t *nasm, ninst_profile_t **ninst_profile, int num_device, float **W, float *W_avg);
void heft_gen_B(nasm_t *nasm, network_profile_t *network_profile, int num_device, float **B, float *B_avg);
void heft_gen_L(nasm_t *nasm, network_profile_t *network_profile, int num_device, float *L, float *L_avg);
void heft_gen_C_avg(nasm_t *nasm, float L_avg, float **data, float B_avg, int **dependency, float **C_avg);
void gen_rank_upward(nasm_t *nasm, float *W_avg, float **C_avg, int **dependency, float *rank_upward);
void gen_rank_downward(nasm_t *nasm, float *W_avg, float **C_avg, int **dependency, float *rank_downward);
float calc_rank_upward_rec(nasm_t *nasm, float *W_avg, float **C_avg, int **dependency, float *rank_upward, int target_idx);
float calc_rank_downward_rec(nasm_t *nasm, float *W_avg, float **C_avg, int **dependency, float *rank_downward, int target_idx);

void spinn_model_splitter(spinn_scheduler_t* spinn_scheduler, nasm_t* nasm, int device_idx);

sched_processor_t *heft_init_processor(int num_processor);
sched_task_t *heft_init_task(int num_ninst);
float heft_earliest_idle(sched_processor_t *sched_processor, float min_limit, float duration);
void heft_push_task(sched_processor_t *sched_processor, sched_task_t *sched_task);

int compare_by_rank_upward(const void *ninst_1, const void *ninst_2);

float get_eft_edge(dynamic_scheduler_t* dynamic_scheduler, rpool_t* rpool, int device_idx, int num_dse, int num_parent_ninsts);
float get_eft_server(dynamic_scheduler_t* dynamic_scheduler, networking_engine* net_engine, int device_idx, int net_tx_queue_bytes);

void save_schedule(sched_processor_t *sched_processor_arr, int num_device, char *file_path);
sched_processor_t *load_schedule(char *file_path);
void share_schedule(sched_processor_t **sched_processor_arr, int num_device, DEVICE_MODE device_mode, int server_sock, int client_sock);
void apply_schedule_to_nasm(nasm_t *nasm, sched_processor_t *sched_processor, int num_device, DEVICE_MODE device_mode);

void fl_init(nasm_t *nasm);
fl_path_t *fl_create_path(nasm_t *nasm, ninst_t **last_layer_ninsts, unsigned int num_last_layer_ninsts);
int fl_is_ninst_in_path_layer(fl_path_layer_t *path_layer, ninst_t *ninst);
void fl_push_path_ninsts(rpool_t *rpool, fl_path_t *path);
void fl_push_path_ninsts_edge(rpool_t *rpool, fl_path_t *path);
void fl_push_path_ninsts_server(rpool_t *rpool, fl_path_t *path);
void fl_push_path_ninsts_until(rpool_t *rpool, fl_path_t *path, unsigned int last_layer_idx);
void fl_push_ninsts_after(rpool_t *rpool, nasm_t *nasm, unsigned int last_layer_idx, unsigned int to_group);
void fl_push_ninsts_only(rpool_t *rpool, nasm_t *nasm, unsigned int layer_idx, unsigned int to_group);
void fl_set_dev_compute(nasm_t *nasm, fl_path_t *path, DEVICE_MODE dev_mode);

#endif