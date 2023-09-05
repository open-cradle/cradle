def get_tasklets_in_pool(json, pool_name):
    local_json = [m for m in json['machines'] if m['machine_name'] == 'local'][0]
    return [t for t in local_json['tasklets'] if t['pool_name'] == pool_name]
