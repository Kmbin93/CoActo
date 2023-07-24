./main_scheduling \
    --sock_type=1   \
    --pipelined=1  \
    --dirname=temp  \
    --target_config="data/cfg/vgg16_aspen.cfg"   \
    --target_nasm_dir="data/vgg16_B1.nasm" \
    --target_bin="data/vgg16_data.bin" \
    --target_input="data/batched_input_128.bin" \
    --prefix="vgg16_B1" \
    --rx_ip="127.0.0.1" \
    --rx_port=3786   \
    --schedule_policy="local"  \
    --sched_sequential_idx=1    \
    --dse_num=16    \
    --output_order="cnn"    \
    --inference_repeat_num=10