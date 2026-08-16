#!/bin/bash

if [ "$#" -ne 2 ]; then
<<<<<<< HEAD
    echo "USE: ./logs.sh <Total UEs in experiment> <Total Proxy Nodes in Experiment>"
=======
    echo "USE: ./save-logs.sh <Total UEs in experiment> <Total Proxy Nodes in Experiment>"
>>>>>>> fbfb90238a55b49c212ad7aef107b5d8185555f3
    exit 1
fi

NUM_UES=$1
NUM_PROXY=$2

# Make a directory to store the logs
DIR="${NUM_UES}_$( date +'%F_%H-%M-%S.%N')"
echo "Saving logs in directory $DIR"
mkdir $DIR

export NUM_UES
export NUM_PROXY
export DIR

save_ue_log() {
    kubectl logs -c ue "ue-$1" > "$DIR/ue-$1-main.log"
<<<<<<< HEAD
    kubectl logs -c ping "ue-$1" > "$DIR/ue-$1-ping.log"
=======
    kubectl logs -c dp "ue-$1" > "$DIR/ue-$1-dp.log"
>>>>>>> fbfb90238a55b49c212ad7aef107b5d8185555f3
}

save_core_log() {
    kubectl exec -c core "core-$1" -- cat /core.log > "$DIR/core-$1.log"
}

echo "Saving UE logs..."
export -f save_ue_log
seq 0 $(($NUM_UES - 1)) | parallel save_ue_log

echo "Saving Core logs..."
export -f save_core_log
seq 0 $(($NUM_PROXY - 1)) | parallel save_core_log

echo "Saving Global Slot Checker logs..."
kubectl logs globalsc > "$DIR/globalsc.log"

echo "Saving snapshot of k8s..."
kubectl get pods -o wide > "$DIR/kubectl.log"
