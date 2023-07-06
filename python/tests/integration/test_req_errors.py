import pytest

import cradle
import params


@pytest.mark.parametrize('remote', params.remote_params, ids=params.remote_ids)
def test_req_no_uuid_in_json(session, remote):
    req_data = \
        {'missing_uuid': 'sample uuid',
         'title': 'sample title',
         'args': {}}

    with pytest.raises(cradle.exceptions.ErrorResponseError) as excinfo:
        session.resolve_request(req_data, remote)
    msg = excinfo.value.response['content']['error']['unknown']
    assert 'no uuid found in JSON' in msg


@pytest.mark.parametrize('remote', params.remote_params, ids=params.remote_ids)
def test_req_unknown_uuid_in_json(session, remote):
    req_data = \
        {'uuid': 'unknown_uuid',
         'title': 'sample title',
         'args': {}}

    with pytest.raises(cradle.exceptions.ErrorResponseError) as excinfo:
        session.resolve_request(req_data, remote)
    msg = excinfo.value.response['content']['error']['unknown']
    assert 'no request registered with uuid unknown_uuid' in msg
