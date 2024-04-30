if [ "$#" -ne 7 ]; then
    echo "Illegal number of parameters"
    echo "Usage: ./run_tests.sh <num_runs> <server_ip> <server_port> <edge_uname> <edge_passwd> <edge_ip> <edge_port>"
    exit 1
fi

# Check if the sleep time is a number
if [ ! -f "../../coacto" ]; then
    echo "Executable coacto does not exist"
    exit 1
fi

dnn_list=("vgg16" "resnet50" "bert_base" "yolov3")
batch=1
policy="dynamic"
s_core=64
bandwidth=100
num_tile_list=(50 100 200 400 800)

total_runs=0
current_run=0
num_runs=$1
server_ip=$2
server_port=$3
edge_uname=$4
edge_passwd=$5
edge_ip=$6
edge_port=$7
server_name=$(uname -n)
unamestr=$(uname -a)
start_time=$(date +%Y-%m-%d-%T)
output_folder="results"
output_log="$output_folder/${start_time}_log.outputs"
output_csv="$output_folder/${start_time}_outputs.csv"

if [ ! -d "$output_folder" ]; then
    mkdir "$output_folder"
fi

echo "Running tests $1 times, at $unamestr"
echo "Running tests $1 times, at $unamestr" > $output_log
echo "    DNNs: ${dnn_list[@]}"
echo "    DNNs: ${dnn_list[@]}" >> $output_log
echo "    Batch sizes: ${batch_list[@]}"
echo "    Batch sizes: ${batch_list[@]}" >> $output_log
echo "    Scheduling policies: ${policy_list[@]}"
echo "    Scheduling policies: ${policy_list[@]}" >> $output_log
echo "    EDGE username: $edge_uname"
echo "    EDGE username: $edge_uname" >> $output_log
echo "    EDGE ip: $edge_ip"
echo "    EDGE ip: $edge_ip" >> $output_log
echo "    EDGE port: $edge_port"
echo "    EDGE port: $edge_port" >> $output_log
echo "    Num tiles: ${num_tile_list[@]}"
echo "    Num tiles: ${num_tile_list[@]}" >> $output_log
echo ""
echo "Script started at $start_time"
echo "Script started at $start_time" >> $output_log
echo ""
echo "" >> $output_log
echo "EDGE_ip,Scheduling Policy,DNN,Batch size,SERVER Core,BW,Num Tile,Total latency (ms),Transmission latency (ms)" >> $output_csv

total_runs=$(( ${#dnn_list[@]} * ${#num_tile_list[@]} ))

# Set TC in EDGE
shell_cmd="ssh $edge_uname@$edge_ip -p $edge_port"
echo "//////////    Set TC in EDGE    //////////" >> $output_log
tc_reset_cmd_wo_ssh="echo $edge_passwd | sudo -S tc qdisc del dev wlan0 root"
tc_reset_cmd="$shell_cmd '$tc_reset_cmd_wo_ssh'"

tc_set_cmd_wo_ssh="echo $edge_passwd | sudo -S tc qdisc add dev wlan0 root handle 1: htb default 6"
tc_set_cmd="$shell_cmd '$tc_set_cmd_wo_ssh'"

tc_set_bw_cmd_wo_ssh="echo $edge_passwd | sudo -S tc class add dev wlan0 parent 1: classid 1:6 htb rate ${bandwidth}mbit"
tc_set_bw_cmd="$shell_cmd '$tc_set_bw_cmd_wo_ssh'"

echo "     $tc_reset_cmd" >> $output_log
echo "     $tc_set_cmd" >> $output_log
echo "     $tc_set_bw_cmd" >> $output_log

eval $tc_reset_cmd
eval $tc_set_cmd
eval $tc_set_bw_cmd

for dnn in "${dnn_list[@]}"
do
    for num_tile in "${num_tile_list[@]}"
    do
        current_run=$((current_run+1))
        nasm_file="${dnn}_B${batch}_T${num_tile}.nasm"
        output_format="cnn"
        #If dnn is bert_base, change output_format to bert
        if [ "$dnn" == "bert_base" ]; then
            output_format="transformer"
        fi
        
        dir_name="Test_SERVER_${server_name}_${start_time}"
        if [ "$policy" != "fl" ]; then
            #s_core+1 because the networking thread is included in the taskset
            server_cmd="taskset -c 0-$(($s_core+1)) ../../coacto --device_mode=0 --dirname=$dir_name --target_nasm_dir="../../data/$nasm_file" --target_dnn_dir="../../data/${dnn}_base.aspen" --target_input="../../data/batched_input_128.bin" --prefix="$dnn" --server_ip="$server_ip"  --server_port="$server_port"  --schedule_policy="$policy" --dse_num=64 --output_order="${output_format}" --inference_repeat_num=$num_runs --num_edge_devices=1"
            edge_cmd_wo_ssh="../../coacto --device_mode=1 --dirname=$dir_name --target_nasm_dir="../../data/$nasm_file" --target_dnn_dir="../../data/${dnn}_base.aspen" --target_input="../../data/batched_input_128.bin" --prefix="$dnn" --server_ip="$server_ip" --server_port="$server_port" --schedule_policy="$policy" --dse_num=6 --output_order="${output_format}" --inference_repeat_num=$num_runs --num_edge_devices=1"
        else
            server_cmd="taskset -c 0-$(($s_core+1)) ../../main_fl $dir_name $dnn $batch $num_tile $num_runs 64 0 $server_ip $server_port"
            edge_cmd_wo_ssh="../../main_fl $dir_name $dnn $batch $num_tile $num_runs 6 1 $server_ip $server_port"
            if [ "$dnn" == "bert_base" ]; then
                server_cmd="taskset -c 0-$(($s_core+1)) ../../coacto --device_mode=0 --dirname=$dir_name --target_nasm_dir="../../data/$nasm_file" --target_dnn_dir="../../data/${dnn}_base.aspen" --target_input="../../data/batched_input_128.bin" --prefix="$dnn" --server_ip="$server_ip"  --server_port="$server_port"  --schedule_policy=conventional --dse_num=64 --output_order="${output_format}" --inference_repeat_num=$num_runs --num_edge_devices=1"
                edge_cmd_wo_ssh="../../coacto --device_mode=1 --dirname=$dir_name --target_nasm_dir="../../data/$nasm_file" --target_dnn_dir="../../data/${dnn}_base.aspen" --target_input="../../data/batched_input_128.bin" --prefix="$dnn" --server_ip="$server_ip" --server_port="$server_port" --schedule_policy=conventional --dse_num=6 --output_order="${output_format}" --inference_repeat_num=$num_runs --num_edge_devices=1"
            fi
        fi

        edge_cmd="$shell_cmd 'cd ~/kmbin/CoActo/mobisys24_artifact/ablation_tiles && $edge_cmd_wo_ssh'"
        
        echo "//////////    SERVER command    //////////" >> $output_log
        echo "    $server_cmd" >> $output_log

        echo "//////////    EDGE command    //////////" >> $output_log
        echo "    $edge_cmd" >> $output_log

        echo "    $(date +%T): DNN $dnn Policy $policy Server core $s_core batch size $batch BW ${bandwidth} ($current_run/$total_runs)"
        echo "    $(date +%T): DNN $dnn Policy $policy Server core $s_core batch size $batch BW ${bandwidth} ($current_run/$total_runs)" >> $output_log

        eval "killall coacto"
        eval $server_cmd 2>&1 | tee temp_server_out.tmp &
        server_pid=$!
        sleep 1

        eval "$shell_cmd 'killall coacto'"
        eval $edge_cmd 2>&1 | tee temp_edge_out.tmp &
        edge_pid=$!

        #Wait max of 10 minutes for EDGE to finish
        wait_time=600
        total_finish=0
        while [ $wait_time -gt 0 ]; do
            if ! kill -0 $edge_pid 2>/dev/null; then
                total_finish=$((total_finish+1))
            fi
            if [ $total_finish -ge 1 ]; then
                break
            fi
            sleep 1
            wait_time=$((wait_time-1))
        done

        #If EDGE is still running, kill it
        for edge_pid in "${edge_pid_list[@]}"
        do
            if kill -0 $edge_pid 2>/dev/null; then
                echo "    $(date +%T): EDGE is still running after 600 seconds, killing it"
                kill -9 $edge_pid
            fi
        done

        server_out=$(cat temp_server_out.tmp)
        rm temp_server_out.tmp
        #Print the output
        echo "//////////    SERVER Output    //////////" >> $output_log
        echo "        $server_out" >> $output_log

        edge_out=$(cat temp_edge_out.tmp)
        rm temp_edge_out.tmp
        edge_time_taken=$(echo $edge_out | grep -oEi "Time measurement run_aspen \([0-9]+\): [0-9.]+ - ([0-9.]+) secs elapsed" | grep -oEi "([0-9.]+) secs elapsed" | grep -oE "[0-9.]+")
        echo "${edge_time_taken}" > time.temp
        total_edge_time=$(awk '{ sum += $1*1000.0 } END { printf "%f", sum }' time.temp)
        avg_edge_time=$(echo "scale=6; $total_edge_time/$1" | bc | awk '{printf "%f", $0}')
        rm time.temp
        edge_transmission_latency=$(echo $edge_out | grep -oEi "Transmission latency : [0-9.]+" | grep -oE "[0-9.]+")
        echo "${edge_transmission_latency}" > trans.temp
        total_transmission_latency=$(awk '{ sum += $1 } END { printf "%f", sum }' trans.temp)
        avg_transmission_latency=$(echo "scale=6; $total_transmission_latency/$1" | bc | awk '{printf "%f", $0}')
        rm trans.temp

        echo "$edge_ip,$policy,$dnn,$batch,$s_core,$bandwidth,$num_tile,$avg_edge_time,$avg_transmission_latency" >> $output_csv

        sleep 1
    done
done