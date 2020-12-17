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
def exec_sshCommand_aca(host, user, password, cmd, timeout=60):
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
            # result['data'].append(out1)
            print(f'Output: {out2.decode()}')
        ssh.close()  # close ssh connection
        return result
    except Exception as e:
        print(f'Exception when executing command:{e}')
        result['error'] = True
        return result


def talk_to_zeta(file_path, zgc_api_url, zeta_data, port_api_upper_limit, time_interval_between_calls_in_seconds):
    headers = {'Content-type': 'application/json'}
    # create ZGC
    ZGC_data = zeta_data["ZGC_data"]
    print(f'ZGC_data: \n{ZGC_data}')
    zgc_response = requests.post(
        zgc_api_url + "/zgcs", data=json.dumps(ZGC_data), headers=headers)
    print(f'zgc creation response: \n{zgc_response}')
    zgc_id = zgc_response.json()['zgc_id']

    # add Nodes
    for node in zeta_data["NODE_data"]:
        node_data = node
        node_data['zgc_id'] = zgc_id
        print(f'node_data: \n{node_data}')
        node_response_data = requests.post(
            zgc_api_url + "/nodes", data=json.dumps(node_data), headers=headers)
        print(f'Response for adding node: {node_response_data.text}')

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
        vpc_response_data = requests.post(
            zgc_api_url + "/vpcs", data=json.dumps(VPC_data), headers=headers).json()
        print(f'Response for adding VPC: {vpc_response_data}')
        json_content_for_aca['vpc_response'] = (vpc_response_data)

    # second delay
    # TODO: Check if this can be removed.
    print('Sleep 60 seconds after the VPC call')
    time.sleep(60)
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
            return
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
        itertools.chain.from_iterable(all_post_responses))
    print(
        f'Length of all ports response: {len(json_content_for_aca["port_response"])}')
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
            "mac_node": "e8:bd:d1:01:77:ec"
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
        "mac_node": "e8:bd:d1:01:72:c8"
    }


def generate_ports(ports_to_create):
    print(f'Need to generate {ports_to_create} ports')
    node_data = {}
    all_ports_generated = []
    for i in range(ports_to_create):
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
    return all_ports_generated

# To run the pseudo controller, the user either runs it without specifying how many ports to create, which leads to creating 2 ports and running the
# DISABLED_zeta_gateway_path_CHILD and DISABLED_zeta_gateway_path_PARENT; if you specify the amount of ports to create (up to one milliion ports), using the command 'python3 run.py amount_of_ports_to_create', the controller will that many ports, and then run DISABLED_zeta_scale_CHILD and DISABLED_zeta_scale_PARENT

# Also, two more params are added.
# First is port_api_upper_limit, which should not exceed 4000, it is the batch number for each /ports POST call.
# Second is time_interval_between_calls_in_seconds, it is the time the pseudo controller sleeps after each /port POST call, except for the last call.

# So if you only want to run the two nodes test, you can simply run 'python3 run.py'
# If you want to try to scale test, you can run 'python3 run.py total_amount_of_ports how_many_ports_each_batch, how_many_seconds_controller_sleeps_after_each_call.'


def run():
    port_api_upper_limit = 1000
    time_interval_between_calls_in_seconds = 10
    ports_to_create = 2
    # right now the only argument should be how many ports to be generated.
    arguments = sys.argv
    print(f'Arguments: {arguments}')
    file_path = './data/zeta_data.json'
    zeta_data = {}
    with open(file_path, 'r', encoding='utf8')as fp:
        zeta_data = json.loads(fp.read())

    server_aca_repo_path = zeta_data['server_aca_repo_path']
    print(f'Server aca repo path: {server_aca_repo_path}')
    zgc_api_url = zeta_data["zeta_api_ip"]

    testcases_to_run = ['DISABLED_zeta_gateway_path_CHILD',
                        'DISABLED_zeta_gateway_path_PARENT']
    # second argument should be amount of ports to be generated
    if len(arguments) > 1:
        ports_to_create = int(arguments[1])
        if ports_to_create > 1000000:
            print(
                f'You tried to create {ports_to_create} ports, but the pseudo controller only supports up to 1,000,000 ports, sorry.')
            return
        print("Has arguments, need to generate some ports!")
        if ports_to_create > 2:
            print(f'Trying to create {ports_to_create} ports.')
            testcases_to_run = ['DISABLED_zeta_scale_CHILD',
                                'DISABLED_zeta_scale_PARENT']
            zeta_data['PORT_data'] = generate_ports(ports_to_create)
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

    json_content_for_aca = talk_to_zeta(file_path, zgc_api_url, zeta_data,
                 port_api_upper_limit, time_interval_between_calls_in_seconds)

    aca_nodes_data = zeta_data["aca_nodes"]
    aca_nodes_ip = aca_nodes_data['ip']

    res = upload_file_aca(aca_nodes_data['ip'], aca_nodes_data['username'], aca_nodes_data['password'],
                          server_aca_repo_path + aca_data_destination_path, aca_data_local_path)
    if not res:
        print("upload file %s failed" % aca_data_local_path)
    else:
        print("upload file %s successfully" % aca_data_local_path)

    test_start_time = time.time()
    # Execute remote command, use the transferred file to change the information in aca_test_ovs_util.cpp,recompile using 'make',perform aca_test
    aca_nodes = aca_nodes_ip
    cmd_list2 = [
        f'cd {server_aca_repo_path};sudo ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*{testcases_to_run[0]}']
    # t1 = threading.Thread(target=exec_sshCommand_aca, args=(
    #     aca_nodes[1], aca_nodes_data['username'], aca_nodes_data['password'], cmd_list2, 1500))

    cmd_list1 = [
        f'cd {server_aca_repo_path};sudo ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*{testcases_to_run[1]}']

    # t2 = threading.Thread(target=exec_sshCommand_aca, args=(
    #     aca_nodes[0], aca_nodes_data['username'], aca_nodes_data['password'], cmd_list1, 1500))

    # t1.start()
    # t2.start()

    # t1.join()
    # t2.join()
    # param_list = [
    #                 f'cd {server_aca_repo_path};sudo ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*{testcases_to_run[0]}',
    #                 f'cd {server_aca_repo_path};sudo ./build/tests/aca_tests --gtest_also_run_disabled_tests --gtest_filter=*{testcases_to_run[1]}'
    #             ]
    results = []
    with concurrent.futures.ThreadPoolExecutor() as executor:
        # future = [executor.submit(exec_sshCommand_aca, param) for param in param_list]
        # results = [f.results() for f in futures]
        future_child = executor.submit(exec_sshCommand_aca, aca_nodes[1], aca_nodes_data['username'], aca_nodes_data['password'], cmd_list2, 1500)
        future_parent = executor.submit(exec_sshCommand_aca, aca_nodes[0], aca_nodes_data['username'], aca_nodes_data['password'], cmd_list1, 1500)
        results.append(future_child.result())
        results.append(future_parent.result())
    for result in results:
        print(f'Status: {result["status"]}, data: {result["data"]}, error: {result["error"]}')
    
    test_end_time = time.time()
    print(
        f'Time took for the tests of ACA nodes are {test_end_time - test_start_time} seconds.')
    print('Time for the Ping test')
    

    parent_ports = [port for port in json_content_for_aca['port_response'] if (port['ip_node'].split('.'))[3] == (zeta_data['aca_nodes']['ip'][0].split('.'))[3]]
    child_ports = [port for port in json_content_for_aca['port_response'] if (port['ip_node'].split('.'))[3] == (zeta_data['aca_nodes']['ip'][1].split('.'))[3]]
    ping_result = {}
    if len(parent_ports) > 0 and len(child_ports) > 0:
        ping_cmd = [f'ping -I {parent_ports[0]["ips_port"][0]["ip"]} -c1 {child_ports[0]["ips_port"][0]["ip"]}']
        print(f'Command for ping: {ping_cmd[0]}')
        ping_result = exec_sshCommand_aca(host=aca_nodes[0], user=aca_nodes_data['username'], password=aca_nodes_data['password'], cmd=ping_cmd, timeout=20)
    else:
        print(f'Either parent or child does not have any ports, somethings wrong.')
    print(f'Ping result: {ping_result}')

if __name__ == '__main__':
    run()