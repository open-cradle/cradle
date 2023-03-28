import pytest

import cradle
import params


@pytest.mark.parametrize('remote', params.remote_params, ids=params.remote_ids)
def test_req_no_uuid_in_json(session, remote):
    req_data = \
        {'impl': {'polymorphic_id': 2147483649,
                  'missing_polymorphic_name': 'sample uuid',
                  'ptr_wrapper': {}},
         'title': 'sample title'}

    with pytest.raises(cradle.exceptions.ErrorResponseError) as excinfo:
        session.resolve_request(req_data, remote)
    msg = excinfo.value.response['content']['error']['unknown']
    assert 'no polymorphic_name found in JSON' in msg


@pytest.mark.parametrize('remote', params.remote_params, ids=params.remote_ids)
def test_req_unknown_uuid_in_json(session, remote):
    req_data = \
        {'impl': {'polymorphic_id': 2147483649,
                  'polymorphic_name': 'unknown_uuid',
                  'ptr_wrapper': {}},
         'title': 'sample title'}

    with pytest.raises(cradle.exceptions.ErrorResponseError) as excinfo:
        session.resolve_request(req_data, remote)
    msg = excinfo.value.response['content']['error']['unknown']
    assert 'no request registered with uuid unknown_uuid' in msg
