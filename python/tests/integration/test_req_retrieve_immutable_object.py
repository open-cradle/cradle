import msgpack  # type: ignore

import make_request


def test_req_retrieve_immutable_object(session):
    # This immutable id comes from another test, and the resulting blob
    # is some MessagePack-encoded value
    immutable_id = '61e8255f01c0e555298e8c7360a98955'

    req_data = make_request.make_retrieve_immutable_object_request(session, immutable_id)

    msgpack_encoded = session.resolve_request(req_data)
    assert msgpack_encoded == b'\x93\xa3abc\xa3def\xa3ghi'
    value = msgpack.unpackb(msgpack_encoded, use_list=False, raw=False)
    assert value == ('abc', 'def', 'ghi')
