import pytest

import make_request
import params


@pytest.mark.parametrize('remote', params.remote_params, ids=params.remote_ids)
def test_req_retrieve_immutable_object(session, remote):
    # This immutable id comes from another test, and the resulting blob
    # is some MessagePack-encoded value
    immutable_id = '61e8255f01c0e555298e8c7360a98955'

    req_data = make_request.make_retrieve_immutable_object_request(session, immutable_id)

    value = session.resolve_request(req_data, remote)
    assert value == ('abc', 'def', 'ghi')
