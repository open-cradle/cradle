import make_request


def test_req_get_iss_object_metadata(session):
    # The object is an array of strings: ('abc', 'def', 'ghi')
    object_id = '61e8256000c030b6c41dffee788df15f'

    req_data = make_request.make_get_iss_object_metadata_request(session, object_id)

    metadata = session.resolve_request(req_data)
    assert metadata['Content-Type'] == 'application/octet-stream'
    assert metadata['Thinknode-Reference-Id'] == object_id
    assert metadata['Thinknode-Size'] == '33'
    assert metadata['Thinknode-Type'] == 'array/string'
