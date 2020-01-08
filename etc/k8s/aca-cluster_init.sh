#!/bin/bash

set -euo pipefail

function main() {
    timeout=600
    running="running"

    # Start aca on all nodes.
    kubectl apply -f aca-daemonset.yaml
    validate $running aca-daemonset.yaml aca $timeout
}

function validate() {
    PODS=$(kubectl get pods | grep $3 | awk '{print $1}')
    end=$((SECONDS+$4))

    for POD in ${PODS}; do
        echo -n "Waiting for pod:$POD"
        while [[ $SECONDS -lt $end ]]; do
            if [[ $1 == "running" ]]; then
                check_running $POD || break
            else
                check_done $POD || break
            fi
        done
        echo
        if [[ $SECONDS -lt $end ]]; then
            echo "Pod:${POD} now $1!"
        else
            echo "ERROR: ${POD} timed out after $4 seconds!"
            kubectl delete -f $2
            exit 1
        fi
    done

    echo
    echo "All $3 pods now $1!"
    echo
}

function check_done() {
    if [[ $(kubectl logs $1 --tail 1) == "done" ]]; then
        return 1
    else
        sleep 2
        echo -n "."
        return 0
    fi
}

function check_running() {
    if [[ $(kubectl get pod $1 -o go-template --template "{{.status.phase}}") == "Running" ]]; then
        return 1
    else
        sleep 2
        echo -n "."
        return 0
    fi
}

main