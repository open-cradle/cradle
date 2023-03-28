import pytest

import make_request
import params


@pytest.mark.parametrize('remote', params.remote_params, ids=params.remote_ids)
def test_req_post_iss_object(session, remote):
    print(f'{remote=}')
    # The value and object id come from another test.
    value = ('abc', 'def', 'ghi')

    req_data = make_request.make_post_iss_object_request(session, value)

    object_id = session.resolve_request(req_data, remote)
    assert object_id == '61e8256000c030b6c41dffee788df15f'
