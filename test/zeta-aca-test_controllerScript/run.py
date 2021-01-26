import requests
import json
import paramiko
from collections import defaultdict, OrderedDict
import time
import sys
import itertools
from math import ceil
import threading
import concurrent.futures
import subprocess
from random import randint

server_aca_repo_path = ''
aca_data_destination_path = '/test/gtest/aca_data.json'
aca_data_local_path = './aca_data.json'

ips_ports_ip_prefix = "123."
mac_port_prefix = "6c:dd:ee:"


# Transfer the file locally to aca nodes


def upload_file_aca(host, user, password, server_path, local_path, timeout=600):
    """
    :param host
    :param user
    :param password
    :param server_path: /root/alcor-control-agent/test/gtest
    :param local_path: ./text.txt
    :param timeout
    :return: bool
    """
    try:
        for host_ip in host:
            t = paramiko.Transport((host_ip, 22))
            t.banner_timeout = timeout
            t.connect(username=user, password=password)
            sftp = paramiko.SFTPClient.from_transport(t)
            sftp.put(local_path, server_path)
            t.close()
        return True
    except Exception as e:
        print(e)
        return False


# Execute remote SSH commands
def exec_sshCommand_aca(host, user, password, cmd, timeout=60, output=True):
    """
    :param host
    :param user
    :param password
    :param cmd
    :param seconds
    :return: dict
    """
    result = {'status': [], 'data': [], 'error': False}  # Record return result
    try:
        # Create a SSHClient instance
        ssh = paramiko.SSHClient()
        ssh.banner_timeout = timeout
        # set host key
        ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        # Connect to remote server
        ssh.connect(host, 22, user, password, timeout=timeout)
        for command in cmd:
            # execute command
            print(f'executing command: {command}')
            stdin, stdout, stderr = ssh.exec_command(
                command, get_pty=True, timeout=timeout)
            # If need password
            if 'sudo' in command:
                stdin.write(password + '\n')
            # result of execution,return a list
            # out1 = stdout.readlines()
            out2 = stdout.read()
            # execution state:0 means success,1 means failure
            channel = stdout.channel
            status = channel.recv_exit_status()
            result['status'].append(status)
            decoded_output = out2.decode()
            result['data'].append(decoded_output)
            if output:
                print(f'Output: {decoded_output}')
        ssh.close()  # close ssh connection
        return result
    except Exception as e:
        print(f'Exception when executing command:{e}')
        result['error'] = True
        return result


def talk_to_zeta(file_path, zgc_api_url, zeta_data, port_api_upper_limit, time_interval_between_calls_in_seconds, ports_to_send_to_aca):
    headers = {'Content-type': 'application/json'}
    # create ZGC
    ZGC_data = zeta_data["ZGC_data"]
    print(f'ZGC_data: \n{ZGC_data}')
    zgc_response = requests.post(
        zgc_api_url + "/zgcs", data=json.dumps(ZGC_data), headers=headers)
    print(f'zgc creation response: \n{zgc_response.text}')
    if zgc_response.status_code >= 300:
        print('Failed to create zgc, pseudo controller will stop now.')
        return False
    zgc_id = zgc_response.json()['zgc_id']

    # add Nodes
    for node in zeta_data["NODE_data"]:
        node_data = node
        node_data['zgc_id'] = zgc_id
        print(f'node_data: \n{node_data}')
        node_response_data = requests.post(
            zgc_api_url + "/nodes", data=json.dumps(node_data), headers=headers)
        print(f'Response for adding node: {node_response_data.text}')
        if node_response_data.status_code >= 300:
            print('Failed to create nodes, pseudo controller will stop now.')
            return False

    json_content_for_aca = dict()
    json_content_for_aca['vpc_response'] = {}
    json_content_for_aca['port_response'] = {}

    # first delay
    # TODO: Check if this can be removed.
    print('Sleep 10 seconds after the Nodes call')
    time.sleep(10)

    # add VPC
    for item in zeta_data["VPC_data"]:
        VPC_data = item
        print(f'VPC_data: \n{VPC_data}')
        vpc_response = requests.post(
            zgc_api_url + "/vpcs", data=json.dumps(VPC_data), headers=headers)
        print(f'Response for adding VPC: {vpc_response.text}')
        if vpc_response.status_code >= 300:
            print('Failed to create vpc, pseudo controller will stop now.')
            return False
        json_content_for_aca['vpc_response'] = (vpc_response.json())

    # second delay
    # TODO: Check if this can be removed.
    print('Sleep 10 seconds after the VPC call')
    time.sleep(10)
    print('Start calling /ports API')

    # notify ZGC the ports created on each ACA
    PORT_data = zeta_data["PORT_data"]

    amount_of_ports = len(PORT_data)
    all_post_responses = []
    all_ports_start_time = time.time()
    print(f'Port_data length: \n{amount_of_ports}')
    should_sleep = True
    for i in range(ceil(len(PORT_data) / port_api_upper_limit)):
        start_idx = i * port_api_upper_limit
        end_idx = start_idx
        if end_idx + port_api_upper_limit >= amount_of_ports:
            end_idx = amount_of_ports
        else:
            end_idx = end_idx + port_api_upper_limit

        if start_idx == end_idx:
            end_idx = end_idx + 1

        if end_idx == amount_of_ports:
            should_sleep = False
        print(
            f'In this /ports POST call, we are calling with port from {start_idx} to {end_idx}')
        one_call_start_time = time.time()
        port_response = requests.post(
            zgc_api_url + "/ports", data=json.dumps(PORT_data[start_idx: end_idx]), headers=headers)
        if port_response.status_code >= 300:
            print(
                f'Call failed for index {start_idx} to {end_idx}, \nstatus code: {port_response.status_code}, \ncontent: {port_response.content}\nExiting')
            return False
        one_call_end_time = time.time()

        print(
            f'ONE PORT post call ended, for {end_idx - start_idx} ports creation it took: {one_call_end_time - one_call_start_time} seconds')
        all_post_responses.append(port_response.json())

        if should_sleep:
            time.sleep(time_interval_between_calls_in_seconds)

    all_ports_end_time = time.time()
    print(
        f'ALL PORT post call ended, for {amount_of_ports} ports creation it took: {all_ports_end_time - all_ports_start_time} seconds')
    json_content_for_aca['port_response'] = list(
        itertools.chain.from_iterable(all_post_responses))[:ports_to_send_to_aca]
    print(
        f'Amount of ports to send to aca: {len(json_content_for_aca["port_response"])}')
    with open('aca_data.json', 'w') as outfile:
        json.dump(json_content_for_aca, outfile)
        print(f'The aca data is exported to {aca_data_local_path}')
    return json_content_for_aca

# the ports' info inside are based on the PORT_data in zeta_data.json, please modify it accordingly to suit your needs


def get_port_template(i):
    if i % 2 == 0:
        return {
            "port_id": "333d4fae-7dec-11d0-a765-00a0c9341120",
            "vpc_id": "3dda2801-d675-4688-a63f-dcda8d327f61",
            "ips_port": [
                {
                    "ip": "10.10.0.92",
                    "vip": ""
                }
            ],
            "mac_port": "cc:dd:ee:ff:11:22",
            "ip_node": "192.168.20.92",
            # "ip_node": "172.16.150.221",
            "mac_node": "e8:bd:d1:01:77:ec"
            # "mac_node": "64:6e:97:0d:80:a9"
        }
    return {
        "port_id": "99976feae-7dec-11d0-a765-00a0c9342230",
        "vpc_id": "3dda2801-d675-4688-a63f-dcda8d327f61",
        "ips_port": [
            {
                "ip": "10.10.0.93",
                "vip": ""
            }
        ],
        "mac_port": "6c:dd:ee:ff:11:32",
        "ip_node": "192.168.20.93",
        # "ip_node": "172.16.150.222",
        "mac_node": "e8:bd:d1:01:72:c8"
        # "mac_node": "64:6e:97:1c:8e:65"
    }


def generate_ports(ports_to_create):
    print(f'Need to generate {ports_to_create} ports')
    node_data = {}
    all_ports_generated = []    # Need to skip when i == 0
    i = 0
    while len(all_ports_generated) != ports_to_create:
        if i % 10 != 0:
            port_template_to_use = get_port_template(i)
            port_id = '{0:07d}ae-7dec-11d0-a765-00a0c9341120'.format(i)
            ip_2nd_octet = str((i // 10000))
            ip_3rd_octet = str((i % 10000 // 100))
            ip_4th_octet = str((i % 100))
            ip = ips_ports_ip_prefix + ip_2nd_octet + \
                "." + ip_3rd_octet + "." + ip_4th_octet
            mac = mac_port_prefix + ip_2nd_octet + ":" + ip_3rd_octet + ":" + ip_4th_octet
            port_template_to_use['port_id'] = port_id
            port_template_to_use['ips_port'][0]['ip'] = ip
            port_template_to_use['mac_port'] = mac
            all_ports_generated.append(port_template_to_use)
        i = i + 1
    return all_ports_generated

# To run the pseudo controller, the user either runs it without specifying how many ports to create, which leads to creating 2 ports;
# if you specify the amount of ports to create (up to one milliion ports), using the command 'python3 run.py amount_of_ports_to_create', the controller will that many ports

# Also, three more params are added.
# First is port_api_upper_limit, which should not exceed 4000, it is the batch number for each /ports POST call.
# Second is time_interval_between_calls_in_seconds, it is the time the pseudo controller sleeps after each /port POST call, except for the last call.
# Third is how many ports to send to aca, this amount is defaulted to the 2, if specified, no more than amount_of_ports_to_create will be send to aca. However, we suggest not to set this number to more than 10, as it may significantly slow down the aca nodes, as the amount of ports (also amount of containers to be created on the aca nodes) increases.

# After ports are created and aca data is sent to the aca nodes, testcase DISABLED_zeta_scale_container will be called on the aca nodes, to create the aca data, construct the goalstate accordingly, and spins up containers that reprsents the ports.
# After that, 3 ping tests will be performed from aca parent node to aca child node with random ports selected, which are followed by another 3 similar ping test from the aca child node to aca parent node.

# So if you only want to run the two nodes test, you can simply run 'python3 run.py'
# If you want to try to scale test, you can run 'python3 run.py total_amount_of_ports how_many_ports_each_batch how_many_seconds_controller_sleeps_after_each_call how_many_port_to_send_to_aca'


def run():
    # rebuild zgc nodes kvm and cleanup zeta data
    subprocess.call(
        ['/home/user/ws/zzxgzgz/zeta/deploy/zeta_deploy.sh', '-d',  'lab'])

    port_api_upper_limit = 1000
    time_interval_between_calls_in_seconds = 10
    ports_to_create = 2
    # right now the only argument should be how many ports to be generated.
    arguments = sys.argv
    print(f'Arguments: {arguments}')
    file_path = './data/zeta_data.json'
    # file_path = './data/zeta_data_sdn.json'
    zeta_data = {}
    with open(file_path, 'r', encoding='utf8')as fp:
        zeta_data = json.loads(fp.read())

    server_aca_repo_path = zeta_data['server_aca_repo_path']
    print(f'Server aca repo path: {server_aca_repo_path}')
    zgc_api_url = zeta_data["zeta_api_ip"]

    testcases_to_run = ['DISABLED_zeta_scale_container',
                        'DISABLED_zeta_scale_container']
    execute_ping = True
    # second argument should be amount of ports to be generated
    if len(arguments) > 1:
        ports_to_create = int(arguments[1])
        if ports_to_create > 1000000:
            print(
                f'You tried to create {ports_to_create} ports, but the pseudo controller only supports up to 1,000,000 ports, sorry.')
            return
        print("Has arguments, need to generate some ports!")
        if ports_to_create >= 2:
            print(f'Trying to create {ports_to_create} ports.')
            zeta_data['PORT_data'] = generate_ports(ports_to_create)
            execute_ping = True
            print(
                f'After generating ports, we now have {len(zeta_data["PORT_data"])} entries in the PORT_data')
        elif ports_to_create < 2:
            print('Too little ports to create, please enter a bigger number')

    if len(arguments) > 2:
        arg2 = int(arguments[2])
        if arg2 <= 4000:
            port_api_upper_limit = arg2
            print(f'Set the amount of ports in each port call to be {arg2}')
        else:
            print(
                f'You are trying to call the /nodes api with more than {arg2} entries per time, which is too much. Please enter a number no more than 4000.')
            return
    if len(arguments) > 3:
        arg3 = int(arguments[3])
        time_interval_between_calls_in_seconds = arg3
        print(
            f'Set time interval between /nodes POST calls to be {arg3} seconds.')

    ports_to_send_to_aca = 2

    if len(arguments) > 4:
        arg4 = int(arguments[4])
        ports_to_send_to_aca = arg4
        print(
            f'Set amount of ports to sent to aca to be: {ports_to_send_to_aca}')

    json_content_for_aca = talk_to_zeta(file_path, zgc_api_url, zeta_data,
                                        port_api_upper_limit, time_interval_between_calls_in_seconds, ports_to_send_to_aca)

    if json_content_for_aca is False:
        print('Failed to talk to Zeta, pseudo controller will exit now.')

    aca_nodes_data = zeta_data["aca_nodes"]
    aca_nodes_ip = aca_nodes_data['ip']

    res = upload_file_aca(aca_nodes_data['ip'], aca_nodes_data['username'], aca_nodes_data['password'],
                          server_aca_repo_path + aca_data_destination_path, aca_data_local_path)
    if not res:
        print("upload file %s failed" % aca_data_local_path)
        return
    else:
        print("upload file %s successfully" % aca_data_local_path)
        print('Before the Ping test, remove previously created containers on aca nodes, if any.')

    remove_container_cmd = [
        'docker rm -f $(docker ps --filter "label=test=zeta" -aq)']
    aca_nodes = aca_nodes_ip

    exec_sshCommand_aca(
        host=aca_nodes[0], user=aca_nodes_data['username'], password=aca_nodes_data['password'], cmd=remove_container_cmd, timeout=20)
    exec_sshCommand_aca(
        host=aca_nodes[1], user=aca_nodes_data['username'], password=aca_nodes_data['password'], cmd=remove_container_cmd, timeout=20)

    test_start_time = time.time()
    # Execute remote command, use the transferred file to change the information in aca_test_ovs_util.cpp,recompile using 'make',perform aca_test
    cmd_child = [
        f'cd {server_aca_repo_path};sudo ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*{testcases_to_run[0]}']

    cmd_parent = [
        f'cd {server_aca_repo_path};sudo ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*{testcases_to_run[1]}']

    with concurrent.futures.ThreadPoolExecutor() as executor:
        future_child = executor.submit(
            exec_sshCommand_aca, aca_nodes[1], aca_nodes_data['username'], aca_nodes_data['password'], cmd_child, 1500, False)
        future_parent = executor.submit(
            exec_sshCommand_aca, aca_nodes[0], aca_nodes_data['username'], aca_nodes_data['password'], cmd_parent, 1500, False)
        result_child = future_child.result()
        result_parent = future_parent.result()
        text_file_child = open("output_child.log", "w")
        text_file_child.write(result_child['data'][0])
        text_file_child.close()
        text_file_parent = open("output_parent.log", "w")
        text_file_parent.write(result_parent['data'][0])
        text_file_parent.close()
        print("Port set up finished")

    test_end_time = time.time()
    print(
        f'Time took for the tests of ACA nodes are {test_end_time - test_start_time} seconds.')
    if execute_ping:
        print('Time for the Ping test')
        parent_ports = [port for port in json_content_for_aca['port_response'] if (
            port['ip_node'].split('.'))[3] == (zeta_data['aca_nodes']['ip'][0].split('.'))[3]]
        parent_node_containers_names_string = ""
        for port in parent_ports:
            parent_node_containers_names_string = parent_node_containers_names_string + \
                f' con-{port["ips_port"][0]["ip"]}'
        child_ports = [port for port in json_content_for_aca['port_response'] if (
            port['ip_node'].split('.'))[3] == (zeta_data['aca_nodes']['ip'][1].split('.'))[3]]
        child_node_containers_names_string = ""
        for port in child_ports:
            child_node_containers_names_string = child_node_containers_names_string + \
                f' con-{port["ips_port"][0]["ip"]}'
        ping_result = {}
        if len(parent_ports) > 0 and len(child_ports) > 0:
            ping_times = 3
            print(
                f"*************Doing ping from parent: {aca_nodes[0]} to child: {aca_nodes[1]}*************")
            for i in range(ping_times):
                dump_flow_cmd = ['sudo ovs-ofctl dump-flows br-tun']
                br_tun_before_ping = exec_sshCommand_aca(
                    host=aca_nodes[0], user=aca_nodes_data['username'], password=aca_nodes_data['password'], cmd=dump_flow_cmd, timeout=20)
                pinger = parent_ports[randint(0,
                                              len(parent_ports)-1)]["ips_port"][0]["ip"]
                pingee = child_ports[randint(0,
                                             len(child_ports)-1)]["ips_port"][0]["ip"]

                ping_cmd = [f'docker exec con-{pinger} ping -c1 {pingee}']
                print(f'Command for ping: {ping_cmd[0]}')
                ping_result = exec_sshCommand_aca(
                    host=aca_nodes[0], user=aca_nodes_data['username'], password=aca_nodes_data['password'], cmd=ping_cmd, timeout=20)
                br_tun_after_ping = exec_sshCommand_aca(
                    host=aca_nodes[0], user=aca_nodes_data['username'], password=aca_nodes_data['password'], cmd=dump_flow_cmd, timeout=20)
                print(f'Ping succeeded: {ping_result["status"][0] == 0}')
            print(
                f"*************Doing ping from child: {aca_nodes[1]} to parent: {aca_nodes[0]}*************")
            for i in range(ping_times):
                dump_flow_cmd = ['sudo ovs-ofctl dump-flows br-tun']
                br_tun_before_ping = exec_sshCommand_aca(
                    host=aca_nodes[1], user=aca_nodes_data['username'], password=aca_nodes_data['password'], cmd=dump_flow_cmd, timeout=20)
                pinger = child_ports[randint(0,
                                             len(child_ports)-1)]["ips_port"][0]["ip"]
                pingee = parent_ports[randint(0,
                                              len(parent_ports)-1)]["ips_port"][0]["ip"]

                ping_cmd = [f'docker exec con-{pinger} ping -c1 {pingee}']
                print(f'Command for ping: {ping_cmd[0]}')
                ping_result = exec_sshCommand_aca(
                    host=aca_nodes[1], user=aca_nodes_data['username'], password=aca_nodes_data['password'], cmd=ping_cmd, timeout=20)
                br_tun_after_ping = exec_sshCommand_aca(
                    host=aca_nodes[1], user=aca_nodes_data['username'], password=aca_nodes_data['password'], cmd=dump_flow_cmd, timeout=20)
                print(f'Ping succeeded: {ping_result["status"][0] == 0}')
        else:
            print(f'Either parent or child does not have any ports, somethings wrong.')
    print('This is the end of the pseudo controller, goodbye.')


if __name__ == '__main__':
    run()
