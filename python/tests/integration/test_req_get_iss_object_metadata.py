def test_req_get_iss_object_metadata(session):
    # Request metadata
    uuid_base = 'rq_get_iss_object_metadata_func'
    git_version = session.query_requests_meta_info()['git_version']
    uuid = f'{uuid_base}+{git_version}'
    title = 'get_iss_object_metadata'

    # Args
    url = 'https://mgh.thinknode.io/api/v1.0'
    # The object is an array of strings: ('abc', 'def', 'ghi')
    object_id = '61e8256000c030b6c41dffee788df15f'

    req_data = \
        {'impl': {'polymorphic_id': 2147483649,
                  'polymorphic_name': uuid,
                  'ptr_wrapper': {'data': {'args': {'tuple_element0': url,
                                                    'tuple_element1': session.context_id,
                                                    'tuple_element2': object_id},
                                           'uuid': {'value0': uuid}},
                                  'id': 2147483649}},
         'title': title}

    metadata = session.resolve_request(req_data)
    assert metadata['Content-Type'] == 'application/octet-stream'
    assert metadata['Thinknode-Reference-Id'] == object_id
    assert metadata['Thinknode-Size'] == '33'
    assert metadata['Thinknode-Type'] == 'array/string'
