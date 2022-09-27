import base64

import msgpack  # type: ignore


def test_req_post_iss_object(session):
    # Request metadata
    uuid_base = 'rq_post_iss_object_func'
    git_version = session.query_requests_meta_info()['git_version']
    uuid = f'{uuid_base}+{git_version}'
    title = 'post_iss_object'
    result_type = 'string'

    # Args
    url = 'https://mgh.thinknode.io/api/v1.0'
    # The value and object id come from another test.
    value = ('abc', 'def', 'ghi')
    msgpack_encoded = msgpack.packb(value, use_bin_type=True)
    base64_encoded = base64.standard_b64encode(msgpack_encoded)
    ascii_encoded = str(base64_encoded, encoding='ascii')

    req_data = \
        {'impl': {'polymorphic_id': 2147483649,
                  'polymorphic_name': uuid,
                  'ptr_wrapper': {'data': {'args': {'tuple_element0': url,
                                                    'tuple_element1': session.context_id,
                                                    'tuple_element2': 'array/string',
                                                    'tuple_element3': {
                                                        'size': len(msgpack_encoded),
                                                        'blob': ascii_encoded
                                                    }},
                                           'uuid': {'value0': uuid}},
                                  'id': 2147483649}},
         'title': title}

    object_id = session.resolve_request(result_type, req_data)
    assert object_id == '61e8256000c030b6c41dffee788df15f'
