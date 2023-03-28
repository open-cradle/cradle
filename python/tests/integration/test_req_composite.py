import msgpack  # type: ignore
import pytest

import make_request
import params


@pytest.mark.parametrize('remote', params.remote_params, ids=params.remote_ids)
def test_req_composite(session, remote):
    input_value = ('abc', 'def', 'ghi')

    # Resolving req0 yields an object_id
    req0_data = make_request.make_post_iss_object_request(session, input_value, 4)

    # Resolving req1 yields an immutable_id
    req1_data = make_request.make_resolve_iss_object_to_immutable_request(session, req0_data, 2)

    # Resolving req2 yields input_value, MessagePack-encoded
    req2_data = make_request.make_retrieve_immutable_object_request(session, req1_data, 0)

    msgpack_encoded = session.resolve_request(req2_data, remote)
    assert msgpack_encoded == b'\x93\xa3abc\xa3def\xa3ghi'
    output_value = msgpack.unpackb(msgpack_encoded, use_list=False, raw=False)
    assert output_value == input_value
