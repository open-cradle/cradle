import pytest

import make_request
import params


@pytest.mark.parametrize('remote', params.remote_params, ids=params.remote_ids)
def test_req_get_iss_object_metadata(session, remote):
    # The object is an array of strings: ('abc', 'def', 'ghi')
    object_id = '61e8256000c030b6c41dffee788df15f'

    req_data = make_request.make_get_iss_object_metadata_request(session, object_id)

    metadata = session.resolve_request(req_data, remote)
    assert metadata['Content-Type'] == 'application/octet-stream'
    assert metadata['Thinknode-Reference-Id'] == object_id
    assert metadata['Thinknode-Size'] == '33'
    assert metadata['Thinknode-Type'] == 'array/string'
