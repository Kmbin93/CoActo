PIPELINED=1
DIRNAME="temp"
TARGET_CONFIG="data/cfg/vgg16_aspen.cfg"
TARGET_NASM_DIR="data/vgg16_B1_aspen.nasm"
TARGET_BIN="data/vgg16/vgg16_data.bin"
TARGET_INPUT="data/resnet50/batched_input_64.bin"
PREFIX="vgg16_B1"
RX_IP="192.168.1.176"
RX_PORT=3786
SCHEDULE_POLICY="dynamic"
SCHED_SEQUENTIAL_IDX=1
DSE_NUM=16
OUTPUT_ORDER="cnn"
INFERENCE_REPEAT_NUM=10