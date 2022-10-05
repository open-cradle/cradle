import make_request


def test_req_resolve_iss_object_to_immutable(session):
    # object_id and immutable_id come from another test
    object_id = '61e8256000c030b6c41dffee788df15f'

    req_data = make_request.make_resolve_iss_object_to_immutable_request(session, object_id)

    immutable_id = session.resolve_request(req_data)
    assert immutable_id == '61e8255f01c0e555298e8c7360a98955'
