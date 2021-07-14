# Usage: python3 analyze.py ./aca_200_ports_async_83.log ./ncm_log_200_async_83.log ./tc_log_200_async_83.log

import sys
from statistics import median, mean
import argparse
print("Hi")

ACA_LOG_FILE_NAME = ''

NCM_LOG_FILE_NAME = ''

TC_LOG_FILE_NAME = ''

ACA_LINES = []

TC_LINES = []

NCM_LINES = []


def extract_lines(keyword, lines):
    l = [line for line in lines if keyword in line]
    print(f'For keyword [{keyword}], there are {len(l)} lines')
    return l


def analyze_lines(keyword, description, lines, service_name):

    keyword_lines = extract_lines(keyword, lines)

    keyword_ms = []

    for keyword_line in keyword_lines:
        # print(gs_process_line)
        splited_line = keyword_line.split(' ')
        ms_position = 0
        if service_name == 'aca':
            # print(f'Line: {keyword_line}')
            for index, line_word in enumerate(splited_line):
                if 'milliseconds' in line_word:
                    ms_position = index
                    break
            # print(f'Trying to convert {splited_line[ms_position-1]} to float')
            # print(ms_position)
            # print(f'{splited_line[ms_position - 1]}')
            if ms_position - 1 >= 0:
                keyword_ms.append(float(splited_line[ms_position - 1]))
        elif service_name == 'ncm':
            for index, line_word in enumerate(splited_line):
                if 'seconds' in line_word:
                    ms_position = index
                    break
            keyword_ms.append(float(splited_line[ms_position + 1]))
        elif service_name == 'tc':
            for index, line_word in enumerate(splited_line):
                if 'time=' in line_word:
                    ms_position = index
                    break
            keyword_ms.append(float(splited_line[ms_position].strip('time=')))
    if len(keyword_ms) > 0:

        time_median = median(keyword_ms)

        time_mean = mean(keyword_ms)

        time_max = max(keyword_ms)

        time_min = min(keyword_ms)

        print(
            f'For {description}, the min is {time_min} ms, max is {time_max} ms, mean is {time_mean:.3f} ms, median is {time_median}')
        if service_name == 'tc':
            print('Ping speed is as follows:')
            for ms in keyword_ms:
                print(ms)
            print('Done printing ping speed')
    return


def analyze_aca_log():
    with open(ACA_LOG_FILE_NAME) as f:
        lines = [line.rstrip('\n') for line in f]
        global ACA_LINES
        ACA_LINES = lines

    print(f'This file has {len(lines)} lines')

    gs_keyword = 'Elapsed time for update goalstate operation took:'

    gs_descriptions = 'On-demand GS process time'

    analyze_lines(gs_keyword, gs_descriptions, lines, 'aca')

    t3_keyword = 'T3'

    t3_description = 'On-demand call send to receive time (T3 - T1)'

    analyze_lines(t3_keyword, t3_description, lines, 'aca')

    port_setup_keyword = '[METRICS] Elapsed time for port operation took:'

    port_description = 'Port GS Process Time'

    analyze_lines(port_setup_keyword, port_description, lines, 'aca')

    neighbor_setup_keyword = '[METRICS] Elapsed time for neighbor operation took:'

    neighbor_description = 'Neighbor GS Process Time'

    analyze_lines(neighbor_setup_keyword, neighbor_description, lines, 'aca')

    return


def analyze_ncm_log():
    with open(NCM_LOG_FILE_NAME) as f:
        lines = [line.rstrip('\n') for line in f]
        global NCM_LINES
        NCM_LINES = lines

    print(f'This file has {len(lines)} lines')

    host_operation_keyword = 'From received hostOperation to before response'
    host_operation_description = 'On-demand call received to response sent time (~T3 - T2)'

    analyze_lines(host_operation_keyword,
                  host_operation_description, lines, 'ncm')

    retrieve_vpc_vni_keyword = 'retrieved vpc resource metadata, elapsed Time in milli seconds:'
    retrieve_vpc_vni_description = 'Retrieve VPC resource metadata time'

    analyze_lines(retrieve_vpc_vni_keyword,
                  retrieve_vpc_vni_description, lines, 'ncm')
    return


def analyze_tc_log():
    with open(TC_LOG_FILE_NAME) as f:
        lines = [line.rstrip('\n') for line in f]
        global TC_LINES
        TC_LINES = lines

    total_pings = len(
        [l for l in lines if 'Need to execute this command concurrently:' in l])
    successful_pings = len(
        [l for l in lines if ' 0% ' in l])
    failed_pings = len(
        [l for l in lines if ' 100% ' in l])
    print(
        f'This file has {len(lines)} lines, total pings: {total_pings}, successful_pings: {successful_pings}, failed pings: {failed_pings}')
    ping_speed_keyword = 'bytes from'
    ping_speed_description = 'Ping Speed'
    analyze_lines(ping_speed_keyword, ping_speed_description, lines, 'tc')
    return


def analyze_grpc_latency():
    uuid_set = set()
    neighbor_id_set = set()
    # print(len(ACA_LINES))
    aca_call_ncm_lines = [
        line for line in ACA_LINES if 'on-demand sent on' in line]
    # print(f'There are {len(aca_call_ncm_lines)} lines in aca_call_ncm')
    aca_received_neighbor_id_lines = [
        line for line in ACA_LINES if 'Neighbor ID: ' in line and 'received at:' in line]
    # print(aca_received_neighbor_id_lines)
    for l in aca_call_ncm_lines:
        for element in l.split(' '):
            if '[' in element and ']' in element:
                a = element
                a = a.replace('[', '')
                a = a.replace(']', '')
                a = a.replace(',', '')
                uuid_set.add(a)
    for l in aca_received_neighbor_id_lines:
        splited_line = l.split(' ')
        for index, line_word in enumerate(splited_line):
            if 'ID:' in line_word:
                neighbor_id_set.add(splited_line[index+1])
                break
    print(f'This ACA log file has {len(neighbor_id_set)} neighbor IDs')
    print(f'This ACA log file has {len(uuid_set)} on-demand call UUIDs')
    uuid_aca_call_ncm_time_dict = dict()
    uuid_aca_received_ncm_reply_time_dict = dict()
    uuid_ncm_received_host_operation_request_time_dict = dict()
    uuid_ncm_replied_host_operation_request_time_dict = dict()
    uuid_aca_received_ncm_reply_to_packet_out_time_dict = dict()

    neighbor_id_ncm_send_on_demand_gs_time_dict = dict()
    neigbhor_id_aca_received_on_demand_gs_time_dict = dict()

    for line in ACA_LINES:
        if 'on-demand sent on' in line:
            for uuid in uuid_set:
                if uuid in line:
                    ms_position = 0
                    splited_line = line.split(' ')
                    for index, line_word in enumerate(splited_line):
                        if 'sent' in line_word:
                            ms_position = index
                            break
                    try:
                        ms_float = float(splited_line[ms_position+2])
                        uuid_aca_call_ncm_time_dict[uuid] = ms_float
                    except Exception:
                        print(f'Line: {line}')
                        print(
                            f'Tried to convert index {ms_position+2}, word {splited_line[ms_position+2]} but failed')
                        continue
                    break
        elif 'NCM called returned at' in line:
            for uuid in uuid_set:
                if uuid in line:
                    ms_position = 0
                    splited_line = line.split(' ')
                    for index, line_word in enumerate(splited_line):
                        if 'returned' in line_word:
                            ms_position = index
                            break
                    try:
                        ms_float = float(splited_line[ms_position+2])
                        uuid_aca_received_ncm_reply_time_dict[uuid] = ms_float
                    except Exception:
                        print(f'Line: {line}')
                        print(
                            f'Tried to convert index {ms_position+2}, word {splited_line[ms_position+2]} but failed')
                        continue
                    break
        elif 'processing a successful host operation reply took' in line:
            for uuid in uuid_set:
                if uuid in line:
                    ms_position = 0
                    splited_line = line.split(' ')
                    for index, line_word in enumerate(splited_line):
                        if 'milliseconds' in line_word:
                            ms_position = index
                            break
                    try:
                        ms_float = float(splited_line[ms_position-1])
                        uuid_aca_received_ncm_reply_to_packet_out_time_dict[uuid] = ms_float
                    except Exception:
                        print(f'Line: {line}')
                        print(
                            f'Tried to convert index {ms_position-1}, word {splited_line[ms_position-1]} but failed')
                        continue
                    break
        elif 'Neighbor ID:' in line and 'received at' in line:
            for neighbor_id in neighbor_id_set:
                if neighbor_id in line:
                    ms_position = 0
                    splited_line = line.split(' ')
                    for index, line_word in enumerate(splited_line):
                        if 'at:' in line_word:
                            ms_position = index
                    try:
                        ms_float = float(splited_line[ms_position + 1])
                        neigbhor_id_aca_received_on_demand_gs_time_dict[neighbor_id] = ms_float
                    except Exception:
                        print(f'Line: {line}')
                        print(
                            f'Tried to convert index {ms_position+1}, word {splited_line[ms_position+1]} but failed')
                        continue
                    break

    for line in NCM_LINES:
        if 'received HostRequest with UUID' in line:
            for uuid in uuid_set:
                if uuid in line:
                    ms_position = 0
                    splited_line = line.split(' ')
                    for index, line_word in enumerate(splited_line):
                        if 'at:' in line_word:
                            ms_position = index
                            break
                    try:
                        ms_float = float(splited_line[ms_position+1])
                        uuid_ncm_received_host_operation_request_time_dict[uuid] = ms_float
                    except Exception:
                        print(
                            f'Tried to convert index {ms_position+1}, which is word {splited_line[ms_position+1]} but failed')
                        continue
        elif 'replying HostRequest with UUID' in line:
            for uuid in uuid_set:
                if uuid in line:
                    ms_position = 0
                    splited_line = line.split(' ')
                    # print(f'Reply hostrequest line: {line}')
                    for index, line_word in enumerate(splited_line):
                        if 'at:' in line_word:
                            ms_position = index
                            break
                    try:
                        ms_float = float(splited_line[ms_position+1])
                        uuid_ncm_replied_host_operation_request_time_dict[uuid] = ms_float
                    except Exception:
                        print(
                            f'Tried to convert index {ms_position+1}, which is word {splited_line[ms_position+1]} but failed')
                        continue
        elif 'Sending neighbor ID:' in line:
            for neighbor_id in neighbor_id_set:
                if neighbor_id in line:
                    ms_position = 0
                    splited_line = line.split(' ')
                    for index, line_word in enumerate(splited_line):
                        if 'at:' in line_word:
                            ms_position = index
                    try:
                        ms_float = float(splited_line[ms_position + 1])
                        neighbor_id_ncm_send_on_demand_gs_time_dict[neighbor_id] = ms_float
                    except Exception:
                        print(f'Line: {line}')
                        print(
                            f'Tried to convert index {ms_position+1}, word {splited_line[ms_position+1]} but failed')
                        continue
                    break

    common_uuids = [uuid for uuid in uuid_set if uuid in uuid_aca_call_ncm_time_dict and uuid in uuid_aca_received_ncm_reply_time_dict and uuid in uuid_ncm_received_host_operation_request_time_dict and uuid in uuid_ncm_replied_host_operation_request_time_dict]
    host_operation_request_delay_ms = []
    host_operation_reply_delay_ms = []
    for uuid in common_uuids:
        host_operation_request_delay_ms.append(
            uuid_ncm_received_host_operation_request_time_dict[uuid] - uuid_aca_call_ncm_time_dict[uuid])
        host_operation_reply_delay_ms.append(
            uuid_aca_received_ncm_reply_time_dict[uuid] - uuid_ncm_replied_host_operation_request_time_dict[uuid])
    # print(len(common_uuids))
    if len(host_operation_request_delay_ms) > 0:
        host_operation_request_grpc_delay_median = median(
            host_operation_request_delay_ms)

        host_operation_request_grpc_delay_mean = mean(
            host_operation_request_delay_ms)

        host_operation_request_grpc_delay_max = max(
            host_operation_request_delay_ms)

        host_operation_request_grpc_delay_min = min(
            host_operation_request_delay_ms)

        print(
            f'For host operation request grpc delay, the min is {host_operation_request_grpc_delay_min} ms, max is {host_operation_request_grpc_delay_max} ms, mean is {host_operation_request_grpc_delay_mean:.3f} ms, median is {host_operation_request_grpc_delay_median}')

    if len(host_operation_reply_delay_ms) > 0:
        host_operation_reply_grpc_delay_median = median(
            host_operation_reply_delay_ms)

        host_operation_reply_grpc_delay_mean = mean(
            host_operation_reply_delay_ms)

        host_operation_reply_grpc_delay_max = max(
            host_operation_reply_delay_ms)

        host_operation_reply_grpc_delay_min = min(
            host_operation_reply_delay_ms)

        print(
            f'For host operation reply grpc delay, the min is {host_operation_reply_grpc_delay_min} ms, max is {host_operation_reply_grpc_delay_max} ms, mean is {host_operation_reply_grpc_delay_mean:.3f} ms, median is {host_operation_reply_grpc_delay_median}')

    common_neighbor_ids = [
        neighbor_id for neighbor_id in neighbor_id_set if neighbor_id in neigbhor_id_aca_received_on_demand_gs_time_dict and neighbor_id in neighbor_id_ncm_send_on_demand_gs_time_dict]
    print(
        f'There are totally {len(neighbor_id_set)} unique neighbor IDs, and {len(common_neighbor_ids)} of them appear both in ACA and NCM log')
    # print(neighbor_id_ncm_send_on_demand_gs_time_dict)
    # print(neigbhor_id_aca_received_on_demand_gs_time_dict)
    ncm_push_goalstate_grpc_delays = []
    for neighbor_id in common_neighbor_ids:
        ncm_push_goalstate_grpc_delays.append(
            neigbhor_id_aca_received_on_demand_gs_time_dict[neighbor_id] - neighbor_id_ncm_send_on_demand_gs_time_dict[neighbor_id])
    if len(ncm_push_goalstate_grpc_delays) > 0:
        ncm_push_goalstate_grpc_delay_median = median(
            ncm_push_goalstate_grpc_delays)
        ncm_push_goalstate_grpc_delay_mean = mean(
            ncm_push_goalstate_grpc_delays)
        ncm_push_goalstate_grpc_delay_max = max(ncm_push_goalstate_grpc_delays)
        ncm_push_goalstate_grpc_delay_min = min(ncm_push_goalstate_grpc_delays)
        print(
            f'For ncm push goalstate grpc delay, the min is {ncm_push_goalstate_grpc_delay_min} ms, max is {ncm_push_goalstate_grpc_delay_max} ms, mean is {ncm_push_goalstate_grpc_delay_mean:.3f} ms, median is {ncm_push_goalstate_grpc_delay_median}')
    # print(uuid_aca_received_ncm_reply_to_packet_out_time_dict)
    aca_received_ncm_reply_to_packet_out_time_list = list(
        uuid_aca_received_ncm_reply_to_packet_out_time_dict.values())
    if len(aca_received_ncm_reply_to_packet_out_time_list) > 0:
        aca_received_ncm_reply_to_packet_out_time_median = median(
            aca_received_ncm_reply_to_packet_out_time_list)
        aca_received_ncm_reply_to_packet_out_time_mean = mean(
            aca_received_ncm_reply_to_packet_out_time_list)
        aca_received_ncm_reply_to_packet_out_time_max = max(
            aca_received_ncm_reply_to_packet_out_time_list)
        aca_received_ncm_reply_to_packet_out_time_min = min(
            aca_received_ncm_reply_to_packet_out_time_list)
        print(
            f'For ACA received ncm reply to packet out time, the min is {aca_received_ncm_reply_to_packet_out_time_min} ms, max is {aca_received_ncm_reply_to_packet_out_time_max} ms, mean is {aca_received_ncm_reply_to_packet_out_time_mean:.3f} ms, median is {aca_received_ncm_reply_to_packet_out_time_median}')

    return


arguments = sys.argv

for argument in arguments:
    print(f'Argument: {argument}')

if len(arguments) > 1:
    ACA_LOG_FILE_NAME = arguments[1]
    analyze_aca_log()
else:
    print("Please pass in a log file by it's absolute path, in order to process it.")
    exit(1)

if len(arguments) > 2:
    NCM_LOG_FILE_NAME = arguments[2]
    analyze_ncm_log()
if len(arguments) > 3:
    TC_LOG_FILE_NAME = arguments[3]
    analyze_tc_log()
    analyze_grpc_latency()
