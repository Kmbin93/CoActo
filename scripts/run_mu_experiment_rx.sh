source ./scripts/param_mu.sh

cmd="taskset -c 0-$(($DSE_NUM+1)) ./coacto \
    --device_mode=0   \
    --dirname=${DIRNAME}  \
    --target_dnn_dir=${TARGET_DNN_DIR} \
    --target_nasm_dir=${TARGET_NASM_DIR} \
    --target_input=${TARGET_INPUT} \
    --prefix=${PREFIX} \
    --server_ip=${server_ip} \
    --server_port=${server_port}   \
    --schedule_policy=${SCHEDULE_POLICY}  \
    --dse_num=64    \
    --output_order=${OUTPUT_ORDER}    \
    --inference_repeat_num=${INFERENCE_REPEAT_NUM} \
    --num_edge_devices=${NUM_EDGE_DEVICES}"

echo $cmd
eval $cmd