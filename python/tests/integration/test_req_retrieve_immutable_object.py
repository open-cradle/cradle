import msgpack  # type: ignore


def test_req_retrieve_immutable_object(session):
    # Request metadata
    uuid_base = 'rq_retrieve_immutable_object_func'
    git_version = session.query_requests_meta_info()['git_version']
    uuid = f'{uuid_base}+{git_version}'
    title = 'retrieve_immutable_object'
    result_type = 'blob'

    # Args
    url = 'https://mgh.thinknode.io/api/v1.0'
    # This immutable id comes from another test, and the resulting blob
    # is some MessagePack-encoded value
    immutable_id = '61e8255f01c0e555298e8c7360a98955'

    req_data = \
        {'impl': {'polymorphic_id': 2147483649,
                  'polymorphic_name': uuid,
                  'ptr_wrapper': {'data': {'args': {'tuple_element0': url,
                                                    'tuple_element1': session.context_id,
                                                    'tuple_element2': {'value0': immutable_id}},
                                           'uuid': {'value0': uuid}},
                                  'id': 2147483649}},
         'title': title}

    msgpack_encoded = session.resolve_request(result_type, req_data)
    assert msgpack_encoded == b'\x93\xa3abc\xa3def\xa3ghi'
    value = msgpack.unpackb(msgpack_encoded, use_list=False, raw=False)
    assert value == ('abc', 'def', 'ghi')
